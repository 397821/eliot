/*****************************************************************************
 * Eliot
 * Copyright (C) 1999-2012 Antoine Fraboulet & Olivier Teulière
 * Authors: Antoine Fraboulet <antoine.fraboulet @@ free.fr>
 *          Olivier Teulière <ipkiss @@ gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *****************************************************************************/

#include <boost/foreach.hpp>
#include <sstream>

#include "config.h"
#if ENABLE_NLS
#   include <libintl.h>
#   define _(String) gettext(String)
#else
#   define _(String) String
#endif


#include "dic.h"
#include "tile.h"
#include "rack.h"
#include "round.h"
#include "pldrack.h"
#include "results.h"
#include "player.h"
#include "game.h"
#include "turn_data.h"
#include "encoding.h"
#include "game_exception.h"
#include "turn.h"
#include "cmd/player_rack_cmd.h"
#include "cmd/player_move_cmd.h"
#include "cmd/game_rack_cmd.h"

#include "debug.h"

INIT_LOGGER(game, Game);


Game::Game(const GameParams &iParams, const Game *iMasterGame):
    m_params(iParams), m_masterGame(iMasterGame),
    m_board(m_params), m_bag(iParams.getDic())
{
    m_points = 0;
    m_currPlayer = 0;
}


Game::~Game()
{
    BOOST_FOREACH(Player *p, m_players)
    {
        delete p;
    }
    delete m_masterGame;
}


Player& Game::accessPlayer(unsigned int iNum)
{
    ASSERT(iNum < m_players.size(), "Wrong player number");
    return *(m_players[iNum]);
}


const Player& Game::getPlayer(unsigned int iNum) const
{
    ASSERT(iNum < m_players.size(), "Wrong player number");
    return *(m_players[iNum]);
}


void Game::shuffleRack()
{
    LOG_DEBUG("Shuffling rack for player " << currPlayer());
    PlayedRack pld = getCurrentPlayer().getCurrentRack();
    pld.shuffle();
    m_players[currPlayer()]->setCurrentRack(pld);
}


void Game::reorderRack(const PlayedRack &iNewRack)
{
    LOG_DEBUG("Reordering rack for player " << currPlayer() <<
              " (newRack=" << lfw(iNewRack.toString()) << ")");
    const PlayedRack &pld = getCurrentPlayer().getCurrentRack();

    // Make sure the new rack uses the same letters
    ASSERT(pld.getRack() == iNewRack.getRack(),
           "The old and new racks have different letters" <<
           "(old=" << lfw(pld.toString()) << 
           " new=" << lfw(iNewRack.toString()) << ")");

    m_players[currPlayer()]->setCurrentRack(iNewRack);
}


void Game::realBag(Bag &ioBag) const
{
    // Copy the bag
    ioBag = m_bag;

    vector<Tile> tiles;

    // The real content of the bag depends on the game mode
    if (getMode() == GameParams::kFREEGAME)
    {
        // In freegame mode, take the letters from all the racks
        BOOST_FOREACH(const Player *player, m_players)
        {
            player->getCurrentRack().getAllTiles(tiles);
            BOOST_FOREACH(const Tile &tile, tiles)
            {
                ioBag.takeTile(tile);
            }
        }
    }
    else
    {
        // In training or duplicate mode, take the rack of the current
        // player only
        getPlayer(m_currPlayer).getCurrentRack().getAllTiles(tiles);
        BOOST_FOREACH(const Tile &tile, tiles)
        {
            ioBag.takeTile(tile);
        }
    }
}


bool Game::canDrawRack(const PlayedRack &iPld, bool iCheck, int *reason) const
{
    // When iCheck is true, we must make sure that there are at least 2 vowels
    // and 2 consonants in the rack up to the 15th turn, and at least one of
    // each starting from the 16th turn.
    // So before trying to fill the rack, we'd better make sure there is a way
    // to complete the rack with these constraints...
    unsigned int min = 0;
    if (iCheck)
    {
        // 2 vowels and 2 consonants are needed up to the 15th turn
        if (m_history.getSize() < 15)
            min = 2;
        else
            min = 1;
    }

    // Create a copy of the bag in which we can do everything we want,
    // and take from it the tiles of the players rack so that "bag"
    // contains the right number of tiles.
    Bag bag(getDic());
    realBag(bag);
    // Replace all the tiles of the given rack into the bag
    vector<Tile> tiles;
    iPld.getAllTiles(tiles);
    BOOST_FOREACH(const Tile &tile, tiles)
    {
        bag.replaceTile(tile);
    }

    // Nothing in the rack, nothing in the bag --> end of the (free)game
    if (bag.getNbTiles() == 0)
    {
        if (reason)
            *reason = 1;
        return false;
    }

    // Check whether it is possible to complete the rack properly
    if (bag.getNbVowels() < min ||
        bag.getNbConsonants() < min)
    {
        if (reason)
            *reason = 2;
        return false;
    }

    // In a duplicate game, we need at least 2 letters, even if we have
    // one letter which can be considered both as a consonant and as a vowel
    if (iCheck && bag.getNbTiles() < 2)
    {
        if (reason)
            *reason = 2;
        return false;
    }

    return true;
}


PlayedRack Game::getRackFromMasterGame() const
{
    ASSERT(hasMasterGame(), "No master game defined");

    // End the game when we reach the end of the master game,
    // even if it is still possible to draw a rack
    const unsigned currTurn = getNavigation().getCurrTurn();
    if (currTurn >= m_masterGame->getHistory().getSize())
        throw EndGameException(_("No more turn in the master game"));

    const TurnData &turnData = m_masterGame->getHistory().getTurn(currTurn);
    const PlayedRack &pldRack = turnData.getPlayedRack();
    LOG_INFO("Using rack from master game: " << lfw(pldRack.toString()));

    // Sanity check
    ASSERT(rackInBag(pldRack.getRack(), m_bag),
           "Cannot draw same rack as in the master game");

    return pldRack;
}


Move Game::getMoveFromMasterGame() const
{
    ASSERT(hasMasterGame(), "No master game defined");

    const unsigned currTurn = getNavigation().getCurrTurn();
    // Should never happen (already checked in getRackFromMasterGame())
    ASSERT(currTurn < m_masterGame->getHistory().getSize(),
           "Not enough turns in the master game");

    const TurnData &turnData = m_masterGame->getHistory().getTurn(currTurn);
    const Move &move = turnData.getMove();

    // If the move is not valid, it means we reached the end
    // of the master game. In this case, also end the current game.
    if (!move.isValid())
        throw EndGameException(_("No move defined for this turn in the master game"));

    return move;
}


PlayedRack Game::helperSetRackRandom(const PlayedRack &iPld,
                                     bool iCheck, set_rack_mode mode) const
{
    // If a master game is defined, use it to retrieve the rack
    if (hasMasterGame())
        return getRackFromMasterGame();

    int reason = 0;
    if (!canDrawRack(iPld, iCheck, &reason))
    {
        if (reason == 1)
            throw EndGameException(_("The bag is empty"));
        else if (reason == 2)
            throw EndGameException(_("Not enough vowels or consonants to complete the rack"));
        ASSERT(false, "Error code not handled")
    }

    // When iCheck is true, we must make sure that there are at least 2 vowels
    // and 2 consonants in the rack up to the 15th turn, and at least one of
    // each starting from the 16th turn.
    // So before trying to fill the rack, we'd better make sure there is a way
    // to complete the rack with these constraints...
    unsigned int min = 0;
    if (iCheck)
    {
        // 2 vowels and 2 consonants are needed up to the 15th turn
        if (m_history.getSize() < 15)
            min = 2;
        else
            min = 1;
    }

    // Make a copy of the given rack
    PlayedRack pld = iPld;
    int nold = pld.getNbOld();

    // Create a copy of the bag in which we can do everything we want,
    // and take from it the tiles of the players rack so that "bag"
    // contains the right number of tiles.
    Bag bag(getDic());
    realBag(bag);
    if (mode == RACK_NEW && nold != 0)
    {
        // We may have removed too many letters from the bag (i.e. the 'new'
        // letters of the player)
        vector<Tile> tiles;
        pld.getNewTiles(tiles);
        BOOST_FOREACH(const Tile &tile, tiles)
        {
            bag.replaceTile(tile);
        }
        pld.resetNew();
    }
    else if ((mode == RACK_NEW && nold == 0) || mode == RACK_ALL)
    {
        // Replace all the tiles in the bag before choosing random ones
        vector<Tile> tiles;
        pld.getAllTiles(tiles);
        BOOST_FOREACH(const Tile &tile, tiles)
        {
            bag.replaceTile(tile);
        }
        // RACK_NEW with an empty rack is equivalent to RACK_ALL
        pld.reset();
        // Do not forget to update nold, for the RACK_ALL case
        nold = 0;
    }
    else
    {
        throw GameException(_("Not a random mode"));
    }

    const unsigned int RACK_SIZE = m_params.getRackSize();

    // Get the tiles remaining on the rack
    vector<Tile> tiles;
    pld.getOldTiles(tiles);
    // The rack is already complete, there is nothing to do
    if (tiles.size() >= RACK_SIZE)
    {
        LOG_DEBUG("Rack already complete, nothing to do");
        return iPld;
    }

    bool jokerAdded = false;
    // Are we dealing with a normal game or a joker game?
    if (m_params.hasVariant(GameParams::kJOKER) ||
        m_params.hasVariant(GameParams::kEXPLOSIVE))
    {
        // 1) Is there already a joker in the remaining letters of the rack?
        bool jokerFound = false;
        BOOST_FOREACH(const Tile &tile, tiles)
        {
            if (tile.isJoker())
            {
                jokerFound = true;
                break;
            }
        }

        // 2) If there was no joker, we add one if possible
        if (!jokerFound && bag.contains(Tile::Joker()))
        {
            jokerAdded = true;
            pld.addNew(Tile::Joker());
            tiles.push_back(Tile::Joker());
        }

        // 3) Remove all the jokers from the bag, to avoid taking another one
        while (bag.contains(Tile::Joker()))
        {
            bag.takeTile(Tile::Joker());
        }
    }

    // Handle reject:
    // Now that the joker has been dealt with, we try to complete the rack
    // with truly random tiles. If it meets the requirements (i.e. if there
    // are at least "min" vowels and "min" consonants in the rack), fine.
    // Otherwise, we reject the rack completely, and we try again
    // to complete it, but this time we ensure by construction that the
    // requirements will be met.
    while (bag.getNbTiles() != 0 && pld.getNbTiles() < RACK_SIZE)
    {
        const Tile &l = bag.selectRandom();
        bag.takeTile(l);
        pld.addNew(l);
    }

    if (!pld.checkRack(min, min))
    {
        // Bad luck... we have to reject the rack
        vector<Tile> rejectedTiles;
        pld.getAllTiles(rejectedTiles);
        BOOST_FOREACH(const Tile &rejTile, rejectedTiles)
        {
            bag.replaceTile(rejTile);
        }
        pld.reset();
        // Do not mark the rack as rejected if it was empty
        if (nold > 0)
            pld.setReject();

        // Keep track of the needed consonants and vowels in the rack
        unsigned int neededVowels = min;
        unsigned int neededConsonants = min;

        // Restore the joker if we are in a joker game
        if (jokerAdded)
        {
            pld.addNew(Tile::Joker());
            if (neededVowels > 0)
                --neededVowels;
            if (neededConsonants > 0)
                --neededConsonants;
        }

        // RACK_SIZE - tiles.size() is the number of letters to add to the rack
        if (neededVowels > RACK_SIZE - tiles.size() ||
            neededConsonants > RACK_SIZE - tiles.size())
        {
            // We cannot fill the rack with enough vowels or consonants!
            // Actually this should never happen, but it doesn't hurt to check...
            // FIXME: this test is not completely right, because it supposes no
            // letter can be at the same time a vowel and a consonant
            throw EndGameException("Not enough vowels or consonants to complete the rack");
        }

        // Get the required vowels and consonants first
        for (unsigned int i = 0; i < neededVowels; ++i)
        {
            const Tile &l = bag.selectRandomVowel();
            bag.takeTile(l);
            pld.addNew(l);
            // Handle the case where the vowel can also be considered
            // as a consonant
            if (l.isConsonant() && neededConsonants > 0)
                --neededConsonants;
        }
        for (unsigned int i = 0; i < neededConsonants; ++i)
        {
            const Tile &l = bag.selectRandomConsonant();
            bag.takeTile(l);
            pld.addNew(l);
        }

        // The difficult part is done:
        //  - we have handled joker games
        //  - we have handled the checks
        // Now complete the rack with truly random letters
        while (bag.getNbTiles() != 0 && pld.getNbTiles() < RACK_SIZE)
        {
            const Tile &l = bag.selectRandom();
            bag.takeTile(l);
            pld.addNew(l);
        }
    }

    // In explosive games, we have to perform a search, then replace the
    // joker with the letter providing the best score
    // A joker coming from a previous rack is not replaced
    if (m_params.hasVariant(GameParams::kEXPLOSIVE) && jokerAdded)
    {
        const Rack &rack = pld.getRack();

        MasterResults res(getBag());
        res.search(getDic(), getBoard(), rack,  getHistory().beforeFirstRound());
        if (!res.isEmpty())
        {
            PlayedRack pldCopy = pld;

            // Get the best word
            const Round & bestRound = res.get(0);
            LOG_DEBUG("helperSetRackRandom(): initial rack: "
                      << lfw(pld.toString()) << " (best word: "
                      << lfw(bestRound.getWord()) << ")");

            // Identify the tile we should use to replace the joker
            Tile replacingTile;
            bool jokerUsed = false;
            for (unsigned int i = 0; i < bestRound.getWordLen(); ++i)
            {
                if (bestRound.isJoker(i) && bestRound.isPlayedFromRack(i))
                {
                    const Tile &jokerTile = bestRound.getTile(i);
                    replacingTile = jokerTile.toUpper();
                    jokerUsed = true;
                    break;
                }
            }
            if (!jokerUsed)
            {
                // The joker was not needed for the top. Replace it with a
                // randomly selected tile
                LOG_DEBUG("helperSetRackRandom(): joker not needed for the top");
                replacingTile = bag.selectRandom();
            }

            LOG_DEBUG("helperSetRackRandom(): replacing Joker with "
                      << lfw(replacingTile.toChar()));

            // If the bag does not contain the letter anymore,
            // simply keep the joker in the rack.
            if (bag.contains(replacingTile))
            {
                // The bag contains the replacing letter
                // We need to swap the joker (it is necessarily in the
                // new tiles, because jokerAdded is true)
                Rack tmpRack = pld.getNew();
                ASSERT(tmpRack.contains(Tile::Joker()), "No joker found in the new tiles");
                tmpRack.remove(Tile::Joker());
                tmpRack.add(replacingTile);
                pld.setNew(tmpRack);

                // Make sure the invariant is still correct, otherwise we keep the joker
                if (!pld.checkRack(min, min))
                    pld = pldCopy;
            }
        }
    }

    // Shuffle the new tiles, to hide the order we imposed (joker first in a
    // joker game, then needed vowels, then needed consonants, and rest of the
    // rack)
    pld.shuffleNew();

    // Post-condition check. This should never fail, of course :)
    ASSERT(pld.checkRack(min, min), "helperSetRackRandom() is buggy!");

    return pld;
}


bool Game::rackInBag(const Rack &iRack, const Bag &iBag) const
{
    BOOST_FOREACH(const Tile &t, getDic().getAllTiles())
    {
        if (iRack.count(t) > iBag.count(t))
            return false;
    }
    return true;
}


PlayedRack Game::helperSetRackManual(bool iCheck, const wstring &iLetters) const
{
    if (!getDic().validateLetters(iLetters, L"+-"))
        throw GameException(_("Some letters are invalid for the current dictionary"));

    PlayedRack pld;
    pld.setManual(iLetters);

    const Rack &rack = pld.getRack();
    if (!rackInBag(rack, m_bag))
    {
        throw GameException(_("The bag does not contain all these letters"));
    }

    if (iCheck)
    {
        int min;
        if (m_bag.getNbVowels() > 1 && m_bag.getNbConsonants() > 1
            && m_history.getSize() < 15)
            min = 2;
        else
            min = 1;
        if (!pld.checkRack(min, min))
        {
            throw GameException(_("Not enough vowels or consonants in this rack"));
        }
    }
    return pld;
}


/*********************************************************
 *********************************************************/


unsigned int Game::getNHumanPlayers() const
{
    unsigned int count = 0;
    BOOST_FOREACH(const Player *player, m_players)
    {
        count += (player->isHuman() ? 1 : 0);
    }
    return count;
}


void Game::addPlayer(Player *iPlayer)
{
    ASSERT(iPlayer != NULL, "Invalid player pointer in addPlayer()");

    // The ID of the player is its position in the m_players vector
    iPlayer->setId(getNPlayers());
    m_players.push_back(iPlayer);

    LOG_INFO("Adding player '" << lfw(iPlayer->getName())
             << "' (" << (iPlayer->isHuman() ? "human" : "AI") << ")"
             << " with ID " << iPlayer->getId());
}


void Game::nextPlayer()
{
    ASSERT(getNPlayers() != 0, "Expected at least one player");

    unsigned int newPlayerId;
    if (m_currPlayer == getNPlayers() - 1)
        newPlayerId = 0;
    else
        newPlayerId = m_currPlayer + 1;
    Command *pCmd = new CurrentPlayerCmd(*this, newPlayerId);
    accessNavigation().addAndExecute(pCmd);
}


int Game::checkPlayedWord(const wstring &iCoord,
                          const wstring &iWord,
                          Move &oMove, bool checkRack,
                          bool checkWordAndJunction) const
{
    ASSERT(getNPlayers() != 0, "Expected at least one player");

    // Assume that the move is invalid by default
    const wdstring &dispWord = getDic().convertToDisplay(iWord);
    oMove = Move(dispWord, iCoord);

    if (!getDic().validateLetters(iWord))
        return 1;

    // Init the round with the given coordinates
    Round round;
    round.accessCoord().setFromString(iCoord);
    if (!round.getCoord().isValid())
    {
        return 2;
    }

    // Check the existence of the word
    if (checkWordAndJunction && !getDic().searchWord(iWord))
    {
        return 3;
    }

    // Set the word
    // TODO: make this a Round_ function (Round_setwordfromchar for example)
    // or a Tiles_ function (to transform a char* into a vector<Tile>)
    // Adding a getter on the word could help too...
    vector<Tile> tiles;
    for (unsigned int i = 0; i < iWord.size(); i++)
    {
        tiles.push_back(Tile(iWord[i]));
    }
    round.setWord(tiles);

    // Check the word position, compute its points,
    // and specify the origin of each letter (board or rack)
    int res = m_board.checkRound(round, checkWordAndJunction);
    if (res != 0)
        return res + 4;
    // In duplicate mode, the first word must be horizontal
    if (checkWordAndJunction && m_board.isVacant(8, 8) &&
        (getMode() == GameParams::kDUPLICATE ||
         getMode() == GameParams::kARBITRATION ||
         getMode() == GameParams::kTOPPING))
    {
        if (round.getCoord().getDir() == Coord::VERTICAL)
            return 10;
    }

    if (checkWordAndJunction && checkRack)
    {
        // Check that the word can be formed with the tiles in the rack:
        // we first create a copy of the game rack, then we remove the tiles
        // one by one
        Rack rack = getHistory().getCurrentRack().getRack();

        Tile t;
        for (unsigned int i = 0; i < round.getWordLen(); i++)
        {
            if (round.isPlayedFromRack(i))
            {
                if (round.isJoker(i))
                    t = Tile::Joker();
                else
                    t = round.getTile(i);

                if (!rack.contains(t))
                {
                    return 4;
                }
                rack.remove(t);
            }
        }
    }

    // The move is valid
    oMove = Move(round);

    return 0;
}


void Game::setGameAndPlayersRack(const PlayedRack &iRack, bool iWithNoMove)
{
    // Set the game rack
    Command *pCmd = new GameRackCmd(*this, iRack);
    accessNavigation().addAndExecute(pCmd);
    LOG_INFO("Setting players rack to '" + lfw(iRack.toString()) + "'");
    // All the players have the same rack
    BOOST_FOREACH(Player *player, m_players)
    {
        Command *pCmd = new PlayerRackCmd(*player, iRack);
        accessNavigation().addAndExecute(pCmd);
    }

    if (iWithNoMove)
    {
        // Assign a "no move" pseudo-move to all the players.
        // This avoids the need to distinguish between "has not played yet"
        // and "has played with no move" in duplicate and arbitration modes.
        // This is also practical to know at which turn the warnings, penalties
        // and solos should be assigned.
        BOOST_FOREACH(Player *player, m_players)
        {
            Command *pCmd = new PlayerMoveCmd(*player, Move());
            accessNavigation().addAndExecute(pCmd);
        }
    }
}


Game::CurrentPlayerCmd::CurrentPlayerCmd(Game &ioGame,
                             unsigned int iPlayerId)
    : m_game(ioGame), m_newPlayerId(iPlayerId), m_oldPlayerId(0)
{
}


void Game::CurrentPlayerCmd::doExecute()
{
    m_oldPlayerId = m_game.currPlayer();
    m_game.setCurrentPlayer(m_newPlayerId);
}


void Game::CurrentPlayerCmd::doUndo()
{
    m_game.setCurrentPlayer(m_oldPlayerId);
}


wstring Game::CurrentPlayerCmd::toString() const
{
    wostringstream oss;
    oss << L"CurrentPlayerCmd (new player: " << m_newPlayerId;
    if (isExecuted())
    {
        oss << L"  old player: " << m_oldPlayerId;
    }
    oss << L")";
    return oss.str();
}

