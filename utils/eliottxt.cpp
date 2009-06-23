/*****************************************************************************
 * Eliot
 * Copyright (C) 2005-2009 Antoine Fraboulet & Olivier Teuli�re
 * Authors: Antoine Fraboulet <antoine.fraboulet @@ free.fr>
 *          Olivier Teuli�re <ipkiss @@ gmail.com>
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

#include "config.h"

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <wchar.h>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <locale.h>
#include <wctype.h>
#if HAVE_READLINE_READLINE_H
#   include <stdio.h>
#   include <readline/readline.h>
#   include <readline/history.h>
#endif

#include "dic.h"
#include "dic_exception.h"
#include "header.h"
#include "game_io.h"
#include "game_factory.h"
#include "public_game.h"
#include "training.h"
#include "duplicate.h"
#include "freegame.h"
#include "player.h"
#include "ai_percent.h"
#include "encoding.h"
#include "game_exception.h"
#include "settings.h"


// Use a more friendly type name for the tokenizer
typedef boost::tokenizer<boost::char_separator<wchar_t>,
        std::wstring::const_iterator,
        std::wstring> Tokenizer;

/* A static variable for holding the line. */
static wchar_t *wline_read = NULL;

/**
 * Read a string, and return a pointer to it.
 * Returns NULL on EOF.
 */
wchar_t *rl_gets()
{
#if HAVE_READLINE_READLINE_H
    // If the buffer has already been allocated, return the memory to the free
    // pool
    if (wline_read)
    {
        delete[] wline_read;
    }

    // Get a line from the user
    static char *line_read;
    line_read = readline("commande> ");

    // If the line has any text in it, save it on the history
    if (line_read && *line_read)
        add_history(line_read);

    // Convert the line into wide characters
    // Get the needed length (we _can't_ use string::size())
    size_t len = mbstowcs(NULL, line_read, 0);
    if (len == (size_t)-1)
        return NULL;

    wline_read = new wchar_t[len + 1];
    mbstowcs(wline_read, line_read, len + 1);

    if (line_read)
    {
        free(line_read);
    }
#else
    if (!cin.good())
        return NULL;

    cout << "commande> ";
    string line;
    std::getline(cin, line);
    // Echo the input, to behave like readline and allow playing the
    // non-regression tests
    cout << line << endl;

    // Get the needed length (we _can't_ use string::size())
    size_t len = mbstowcs(NULL, line.c_str(), 0);
    if (len == (size_t)-1)
        return NULL;

    wline_read = new wchar_t[len + 1];
    mbstowcs(wline_read, line.c_str(), len + 1);
#endif

    return wline_read;
}


wstring checkAlphaToken(const vector<wstring> &tokens, uint8_t index)
{
    if (tokens.size() <= index)
        return L"";
    const wstring &wstr = tokens[index];
    BOOST_FOREACH(wchar_t wch, wstr)
    {
        if (!iswalpha(wch))
            return L"";
    }
    return wstr;
}


wstring checkLettersToken(const vector<wstring> &tokens, uint8_t index,
                          const Dictionary &iDic)
{
    if (tokens.size() <= index)
        return L"";
    return iDic.getHeader().convertFromInput(tokens[index]);
}


wstring checkAlphaNumToken(const vector<wstring> &tokens, uint8_t index)
{
    if (tokens.size() <= index)
        return L"";
    const wstring &wstr = tokens[index];
    BOOST_FOREACH(wchar_t wch, wstr)
    {
        if (!iswalnum(wch))
            return L"";
    }
    return wstr;
}


wstring checkNumToken(const vector<wstring> &tokens, uint8_t index)
{
    if (tokens.size() <= index)
        return L"";
    const wstring &wstr = tokens[index];
    BOOST_FOREACH(wchar_t wch, wstr)
    {
        if (!iswdigit(wch) && wch != L'-')
            return L"";
    }
    return wstr;
}


wstring checkFileNameToken(const vector<wstring> &tokens, uint8_t index)
{
    if (tokens.size() <= index)
        return L"";
    const wstring &wstr = tokens[index];
    BOOST_FOREACH(wchar_t wch, wstr)
    {
        if (!iswalpha(wch) && wch != L'.' && wch != L'_')
            return L"";
    }
    return wstr;
}


wstring checkCrossToken(const vector<wstring> &tokens, uint8_t index)
{
    if (tokens.size() <= index)
        return L"";
    const wstring &wstr = tokens[index];
    BOOST_FOREACH(wchar_t wch, wstr)
    {
        if (!iswalpha(wch) && wch != L'.')
            return L"";
    }
    return wstr;
}


void helpTraining()
{
    printf("  ?    : aide -- cette page\n");
    printf("  a [g|l|p|r|t] : afficher :\n");
    printf("            g -- grille\n");
    printf("            gj -- grille + jokers\n");
    printf("            gm -- grille + valeur des cases\n");
    printf("            gn -- grille + valeur des cases (variante)\n");
    printf("            gd -- grille + debug cross (debug only)\n");
    printf("            l -- lettres non jou�es\n");
    printf("            p -- partie\n");
    printf("            pd -- partie (debug)\n");
    printf("            P -- partie (format standard)\n");
    printf("            r -- recherche\n");
    printf("            s -- score\n");
    printf("            S -- score de tous les joueurs\n");
    printf("            t -- tirage\n");
    printf("  d [] : v�rifier le mot []\n");
    printf("  b [b|p|r] [] : effectuer une recherche speciale � partir de []\n");
    printf("            b -- benjamins\n");
    printf("            p -- 7 + 1\n");
    printf("            r -- raccords\n");
    printf("  *    : tirage al�atoire\n");
    printf("  +    : tirage al�atoire ajouts\n");
    printf("  t [] : changer le tirage\n");
    printf("  j [] {} : jouer le mot [] aux coordonn�es {}\n");
    printf("  n [] : jouer le r�sultat num�ro []\n");
    printf("  r    : rechercher les meilleurs r�sultats\n");
    printf("  s [] : sauver la partie en cours dans le fichier []\n");
    printf("  h [p|n|f|l|r] : naviguer dans l'historique (prev, next, first, last, replay\n");
    printf("  q    : quitter le mode entra�nement\n");
}


void helpFreegame()
{
    printf("  ?    : aide -- cette page\n");
    printf("  a [g|l|p|s|t] : afficher :\n");
    printf("            g -- grille\n");
    printf("            gj -- grille + jokers\n");
    printf("            gm -- grille + valeur des cases\n");
    printf("            gn -- grille + valeur des cases (variante)\n");
    printf("            j -- joueur courant\n");
    printf("            l -- lettres non jou�es\n");
    printf("            p -- partie\n");
    printf("            P -- partie (format standard)\n");
    printf("            s -- score\n");
    printf("            S -- score de tous les joueurs\n");
    printf("            t -- tirage\n");
    printf("            T -- tirage de tous les joueurs\n");
    printf("  d [] : v�rifier le mot []\n");
    printf("  j [] {} : jouer le mot [] aux coordonn�es {}\n");
    printf("  p [] : passer son tour en changeant les lettres []\n");
    printf("  s [] : sauver la partie en cours dans le fichier []\n");
    printf("  h [p|n|f|l|r] : naviguer dans l'historique (prev, next, first, last, replay\n");
    printf("  q    : quitter le mode partie libre\n");
}


void helpDuplicate()
{
    printf("  ?    : aide -- cette page\n");
    printf("  a [g|l|p|s|t] : afficher :\n");
    printf("            g -- grille\n");
    printf("            gj -- grille + jokers\n");
    printf("            gm -- grille + valeur des cases\n");
    printf("            gn -- grille + valeur des cases (variante)\n");
    printf("            j -- joueur courant\n");
    printf("            l -- lettres non jou�es\n");
    printf("            p -- partie\n");
    printf("            P -- partie (format standard)\n");
    printf("            s -- score\n");
    printf("            S -- score de tous les joueurs\n");
    printf("            t -- tirage\n");
    printf("  d [] : v�rifier le mot []\n");
    printf("  j [] {} : jouer le mot [] aux coordonn�es {}\n");
    printf("  n [] : passer au joueur n�[]\n");
    printf("  s [] : sauver la partie en cours dans le fichier []\n");
    printf("  h [p|n|f|l|r] : naviguer dans l'historique (prev, next, first, last, replay\n");
    printf("  q    : quitter le mode duplicate\n");
}


void help()
{
    printf("  ?        : aide -- cette page\n");
    printf("  e        : d�marrer le mode entra�nement\n");
    printf("  ej       : d�marrer le mode entra�nement en partie joker\n");
    printf("  ee       : d�marrer le mode entra�nement en partie d�tonante\n");
    printf("  d [] {}  : d�marrer une partie duplicate avec\n");
    printf("                [] joueurs humains et {} joueurs IA\n");
    printf("  dj [] {} : d�marrer une partie duplicate avec\n");
    printf("                [] joueurs humains et {} joueurs IA (partie joker)\n");
    printf("  de [] {} : d�marrer une partie duplicate avec\n");
    printf("                [] joueurs humains et {} joueurs IA (partie d�tonante)\n");
    printf("  l [] {}  : d�marrer une partie libre avec\n");
    printf("                [] joueurs humains et {} joueurs IA\n");
    printf("  lj [] {} : d�marrer une partie libre avec\n");
    printf("                [] joueurs humains et {} joueurs IA (partie joker)\n");
    printf("  le [] {} : d�marrer une partie libre avec\n");
    printf("                [] joueurs humains et {} joueurs IA (partie d�tonante)\n");
    printf("  c []     : charger la partie du fichier []\n");
    printf("  x [] {1} {2} {3} : expressions rationnelles\n");
    printf("          [] expression � rechercher\n");
    printf("          {1} nombre de r�sultats � afficher\n");
    printf("          {2} longueur minimum d'un mot\n");
    printf("          {3} longueur maximum d'un mot\n");
    printf("  s [b|i] {1} {2} : d�finir la valeur {2} pour l'option {1},\n");
    printf("                    qui est de type (b)ool ou (i)nt\n");
    printf("  q        : quitter\n");
}


void displayData(const PublicGame &iGame, const vector<wstring> &tokens)
{
    const wstring &displayType = checkAlphaNumToken(tokens, 1);
    if (displayType == L"")
    {
        cout << "commande incompl�te\n";
        return;
    }
    if (displayType == L"g")
        GameIO::printBoard(cout, iGame);
    else if (displayType == L"gd")
        GameIO::printBoardDebug(cout, iGame);
    else if (displayType == L"gj")
        GameIO::printBoardJoker(cout, iGame);
    else if (displayType == L"gm")
        GameIO::printBoardMultipliers(cout, iGame);
    else if (displayType == L"gn")
        GameIO::printBoardMultipliers2(cout, iGame);
    else if (displayType == L"j")
        cout << "Joueur " << iGame.getCurrentPlayer().getId() << endl;
    else if (displayType == L"l")
        GameIO::printNonPlayed(cout, iGame);
    else if (displayType == L"p")
        iGame.save(cout, PublicGame::kFILE_FORMAT_ADVANCED);
    else if (displayType == L"pd")
        GameIO::printGameDebug(cout, iGame);
    else if (displayType == L"P")
        iGame.save(cout, PublicGame::kFILE_FORMAT_STANDARD);
    else if (displayType == L"r")
    {
        const wstring &limit = checkNumToken(tokens, 2);
        if (limit == L"")
            GameIO::printSearchResults(cout,
                                       iGame.trainingGetResults(),
                                       10);
        else
            GameIO::printSearchResults(cout,
                                       iGame.trainingGetResults(),
                                       _wtoi(limit.c_str()));
    }
    else if (displayType == L"s")
        GameIO::printPoints(cout, iGame);
    else if (displayType == L"S")
        GameIO::printAllPoints(cout, iGame);
    else if (displayType == L"t")
        GameIO::printPlayedRack(cout, iGame, iGame.getHistory().getSize());
    else if (displayType == L"T")
        GameIO::printAllRacks(cout, iGame);
    else
        cout << "commande inconnue\n";
}


void commonCommands(PublicGame &iGame, const vector<wstring> &tokens)
{
    if (tokens[0][0] == L'a')
        displayData(iGame, tokens);
    else if (tokens[0][0] == L'd')
    {
        const wstring &word = checkLettersToken(tokens, 1, iGame.getDic());
        if (word == L"")
            helpDuplicate();
        else
        {
            if (iGame.getDic().searchWord(word))
                printf("le mot -%s- existe\n", convertToMb(word).c_str());
            else
                printf("le mot -%s- n'existe pas\n", convertToMb(word).c_str());
        }
    }
    else if (tokens[0][0] == L'h')
    {
        const wstring &action = checkAlphaToken(tokens, 1);
        wstring count = checkNumToken(tokens, 2);
        if (count == L"")
            count = L"1";
        if (action == L"" || action.size() != 1)
            return;
        if (action[0] == L'p')
        {
            for (int i = 0; i < _wtoi(count.c_str()); ++i)
                iGame.prevTurn();
        }
        else if (action[0] == L'n')
        {
            for (int i = 0; i < _wtoi(count.c_str()); ++i)
                iGame.nextTurn();
        }
        else if (action[0] == L'f')
            iGame.firstTurn();
        else if (action[0] == L'l')
            iGame.lastTurn();
        else if (action[0] == L'r')
            iGame.clearFuture();
    }
    else if (tokens[0][0] == L'j')
    {
        const wstring &word = checkLettersToken(tokens, 1, iGame.getDic());
        if (word == L"")
            helpDuplicate();
        else
        {
            const wstring &coord = checkAlphaNumToken(tokens, 2);
            if (coord == L"")
            {
                helpDuplicate();
                return;
            }
            int res = iGame.play(word, coord);
            if (res != 0)
                printf("Mot incorrect ou mal plac� (%i)\n", res);
        }
    }
    else if (tokens[0][0] == L's')
    {
        const wstring &word = checkFileNameToken(tokens, 1);
        if (word != L"")
        {
            string filename = convertToMb(word);
            ofstream fout(filename.c_str());
            if (fout.rdstate() == ios::failbit)
            {
                printf("impossible d'ouvrir %s\n", filename.c_str());
                return;
            }
            iGame.save(fout);
            fout.close();
        }
    }
}


void handleRegexp(const Dictionary& iDic, const vector<wstring> &tokens)
{
    /*
    printf("  x [] {1} {2} {3} : expressions rationnelles\n");
    printf("          [] expression � rechercher\n");
    printf("          {1} nombre de r�sultats � afficher\n");
    printf("          {2} longueur minimum d'un mot\n");
    printf("          {3} longueur maximum d'un mot\n");
    */

    if (tokens.size() < 2 || tokens.size() > 5)
    {
        printf("Invalid number of parameters\n");
        return;
    }

    const wstring &regexp = tokens[1];
    const wstring &cnres = checkNumToken(tokens, 2);
    const wstring &clmin = checkNumToken(tokens, 3);
    const wstring &clmax = checkNumToken(tokens, 4);

    if (regexp == L"")
        return;

    unsigned int nres = (cnres != L"") ? _wtoi(cnres.c_str()) : 50;
    unsigned int lmin = (clmin != L"") ? _wtoi(clmin.c_str()) : 1;
    unsigned int lmax = (clmax != L"") ? _wtoi(clmax.c_str()) : DIC_WORD_MAX - 1;

    if (lmax > (DIC_WORD_MAX - 1) || lmin < 1 || lmin > lmax)
    {
        printf("bad length -%ls,%ls-\n", clmin.c_str(), clmax.c_str());
        return;
    }

    printf("search for %s (%d,%d,%d)\n", convertToMb(regexp).c_str(),
           nres, lmin, lmax);

    vector<wdstring> wordList;
    try
    {
        iDic.searchRegExp(regexp, wordList, lmin, lmax, nres);
    }
    catch (InvalidRegexpException &e)
    {
        printf("Invalid regular expression: %s\n", e.what());
        return;
    }

    BOOST_FOREACH(const wdstring &wstr, wordList)
    {
        printf("%s\n", convertToMb(wstr).c_str());
    }
    printf("%d printed results\n", wordList.size());
}


void setSetting(const vector<wstring> &tokens)
{
    if (tokens.size() != 4)
    {
        printf("Invalid number of parameters\n");
        return;
    }
    const wstring &type = checkAlphaToken(tokens, 1);
    if (type.size() != 1 ||
        (type[0] != L'b' && type[0] != L'i'))
    {
        printf("Invalid type\n");
        return;
    }
    const wstring &settingWide = tokens[2];
    const wstring &value = tokens[3];

    try
    {
        string setting = convertToMb(settingWide);
        if (type == L"i")
        {
            Settings::Instance().setInt(setting, _wtoi(value.c_str()));
        }
        else if (type == L"b")
        {
            Settings::Instance().setBool(setting, _wtoi(value.c_str()));
        }
    }
    catch (GameException &e)
    {
        string msg = "Error while changing a setting: " + string(e.what()) + "\n";
        printf("%s", msg.c_str());
        return;
    }
}


void loopTraining(PublicGame &iGame)
{
    cout << "mode entra�nement" << endl;
    cout << "[?] pour l'aide" << endl;

    bool quit = 0;
    while (!quit)
    {
        wstring command = rl_gets();
        // Split the command
        vector<wstring> tokens;
        boost::char_separator<wchar_t> sep(L" ");
        Tokenizer tok(command, sep);
        BOOST_FOREACH(const wstring &wstr, tok)
        {
            tokens.push_back(wstr);
        }

        if (tokens.empty())
            continue;
        if (tokens[0].size() > 1)
        {
            printf("%s\n", "Invalid command");
            continue;
        }

        try
        {
            switch (tokens[0][0])
            {
                case L'?':
                    helpTraining();
                    break;
                case L'a':
                case L'd':
                case L'h':
                case L'j':
                case L's':
                    commonCommands(iGame, tokens);
                    break;
                case L'b':
                    {
                        const wstring &type = checkAlphaToken(tokens, 1);
                        if (type == L"")
                            helpTraining();
                        else
                        {
                            const wstring &word = checkLettersToken(tokens, 2, iGame.getDic());
                            if (word == L"")
                                helpTraining();
                            else
                            {
                                switch (type[0])
                                {
                                    case L'b':
                                        {
                                            vector<wdstring> wordList;
                                            iGame.getDic().searchBenj(word, wordList);
                                            BOOST_FOREACH(const wdstring &wstr, wordList)
                                                cout << convertToMb(wstr) << endl;
                                        }
                                        break;
                                    case L'p':
                                        {
                                            map<wdstring, vector<wdstring> > wordMap;
                                            iGame.getDic().search7pl1(word, wordMap, false);
                                            map<wdstring, vector<wdstring> >::const_iterator it;
                                            for (it = wordMap.begin(); it != wordMap.end(); ++it)
                                            {
                                                if (it->first != L"")
                                                    cout << "+" << convertToMb(it->first) << endl;
                                                BOOST_FOREACH(const wdstring &wstr, it->second)
                                                {
                                                    cout << "  " << convertToMb(wstr) << endl;
                                                }
                                            }
                                        }
                                        break;
                                    case L'r':
                                        {
                                            vector<wdstring> wordList;
                                            iGame.getDic().searchRacc(word, wordList);
                                            BOOST_FOREACH(const wdstring &wstr, wordList)
                                                cout << convertToMb(wstr) << endl;
                                        }
                                        break;
                                }
                            }
                        }
                    }
                    break;
                case L'n':
                    {
                        const wstring &num = checkNumToken(tokens, 1);
                        if (num == L"")
                            helpTraining();
                        else
                        {
                            int n = _wtoi(num.c_str());
                            if (n <= 0)
                                printf("mauvais argument\n");
                            iGame.trainingPlayResult(n - 1);
                        }
                    }
                    break;
                case L'r':
                    iGame.trainingSearch();
                    break;
                case L't':
                    {
                        const wstring &letters =
                            checkLettersToken(tokens, 1, iGame.getDic());
                        if (letters == L"")
                            helpTraining();
                        else
                            iGame.trainingSetRackManual(false, letters);
                    }
                    break;
                case L'*':
                    iGame.trainingSetRackRandom(false, PublicGame::kRACK_ALL);
                    break;
                case L'+':
                    iGame.trainingSetRackRandom(false, PublicGame::kRACK_NEW);
                    break;
                case L'q':
                    quit = true;
                    break;
                default:
                    printf("commande inconnue\n");
                    break;
            }
        }
        catch (GameException &e)
        {
            printf("%s\n", e.what());
        }
    }
    printf("fin du mode entra�nement\n");
}


void loopFreegame(PublicGame &iGame)
{
    printf("mode partie libre\n");
    printf("[?] pour l'aide\n");

    bool quit = 0;
    while (!quit)
    {
        wstring command = rl_gets();
        // Split the command
        vector<wstring> tokens;
        boost::char_separator<wchar_t> sep(L" ");
        Tokenizer tok(command, sep);
        BOOST_FOREACH(const wstring &wstr, tok)
        {
            tokens.push_back(wstr);
        }

        if (tokens.empty())
            continue;
        if (tokens[0].size() > 1)
        {
            printf("%s\n", "Invalid command");
            continue;
        }

        try
        {
            switch (tokens[0][0])
            {
                case L'?':
                    helpFreegame();
                    break;
                case L'a':
                case L'd':
                case L'h':
                case L'j':
                case L's':
                    commonCommands(iGame, tokens);
                    break;
                case L'p':
                    {
                        wstring letters = L"";
                        /* You can pass your turn without changing any letter */
                        if (tokens.size() > 1)
                        {
                            letters = checkLettersToken(tokens, 1, iGame.getDic());
                            if (letters == L"")
                                fprintf(stderr, "Invalid letters\n");
                        }
                        // XXX
                        if (iGame.freeGamePass(letters) != 0)
                            break;
                    }
                    break;
                case L'q':
                    quit = true;
                    break;
                default:
                    printf("commande inconnue\n");
                    break;
            }
        }
        catch (GameException &e)
        {
            printf("%s\n", e.what());
        }
    }
    printf("fin du mode partie libre\n");
}


void loopDuplicate(PublicGame &iGame)
{
    printf("mode duplicate\n");
    printf("[?] pour l'aide\n");

    bool quit = false;
    while (!quit)
    {
        wstring command = rl_gets();
        // Split the command
        vector<wstring> tokens;
        boost::char_separator<wchar_t> sep(L" ");
        Tokenizer tok(command, sep);
        BOOST_FOREACH(const wstring &wstr, tok)
        {
            tokens.push_back(wstr);
        }

        if (tokens.empty())
            continue;
        if (tokens[0].size() > 1)
        {
            printf("%s\n", "Invalid command");
            continue;
        }

        try
        {
            switch (tokens[0][0])
            {
                case L'?':
                    helpDuplicate();
                    break;
                case L'a':
                case L'd':
                case L'h':
                case L'j':
                case L's':
                    commonCommands(iGame, tokens);
                    break;
                case L'n':
                    {
                        const wstring &id = checkNumToken(tokens, 1);
                        if (id == L"")
                            helpDuplicate();
                        else
                        {
                            int n = _wtoi(id.c_str());
                            if (n < 0 || n >= (int)iGame.getNbPlayers())
                            {
                                fprintf(stderr, "Num�ro de joueur invalide\n");
                                break;
                            }
                            iGame.duplicateSetPlayer(n);
                        }
                    }
                    break;
                case L'q':
                    quit = true;
                    break;
                default:
                    printf("commande inconnue\n");
                    break;
            }
        }
        catch (GameException &e)
        {
            printf("%s\n", e.what());
        }
    }
    printf("fin du mode duplicate\n");
}


void mainLoop(const Dictionary &iDic)
{
    printf("[?] pour l'aide\n");

    bool quit = false;
    while (!quit)
    {
        wstring command = rl_gets();
        // Split the command
        vector<wstring> tokens;
        boost::char_separator<wchar_t> sep(L" ");
        Tokenizer tok(command, sep);
        BOOST_FOREACH(const wstring &wstr, tok)
        {
            tokens.push_back(wstr);
        }

        if (tokens.empty())
            continue;
        if (tokens[0].size() > 2)
        {
            printf("%s\n", "Invalid command");
            continue;
        }

        try
        {
            switch (tokens[0][0])
            {
                case L'?':
                    help();
                    break;
                case L'c':
                    {
                        const wstring &wfileName = checkFileNameToken(tokens, 1);
                        if (wfileName != L"")
                        {
                            string filename = convertToMb(wfileName);
                            Game *tmpGame = GameFactory::Instance()->load(filename, iDic);
                            if (tmpGame == NULL)
                            {
                                printf("erreur pendant le chargement de la partie\n");
                            }
                            else
                            {
                                PublicGame *game = new PublicGame(*tmpGame);
                                switch (game->getMode())
                                {
                                    case PublicGame::kTRAINING:
                                        loopTraining(*game);
                                        break;
                                    case PublicGame::kFREEGAME:
                                        loopFreegame(*game);
                                        break;
                                    case PublicGame::kDUPLICATE:
                                        loopDuplicate(*game);
                                        break;
                                }
                                //GameFactory::Instance()->releaseGame(*game);
                                delete game;
                            }
                        }
                    }
                    break;
                case L'e':
                    {
                        // New training game
                        Training *tmpGame = GameFactory::Instance()->createTraining(iDic);
                        PublicGame *game = new PublicGame(*tmpGame);
                        // Set the variant
                        if (tokens[0].size() > 1)
                        {
                            if (tokens[0][1] == L'j')
                                game->setVariant(PublicGame::kJOKER);
                            else if (tokens[0][1] == L'e')
                                game->setVariant(PublicGame::kEXPLOSIVE);
                        }
                        game->start();
                        loopTraining(*game);
                        //GameFactory::Instance()->releaseGame(*game);
                        delete game;
                    }
                    break;
                case L'd':
                    {
                        if (tokens.size() != 3)
                        {
                            help();
                            break;
                        }
                        const wstring &nbHuman = checkNumToken(tokens, 1);
                        const wstring &nbAI = checkNumToken(tokens, 2);
                        if (nbHuman == L"" || nbAI == L"")
                        {
                            help();
                            break;
                        }
                        // New duplicate game
                        Duplicate *tmpGame = GameFactory::Instance()->createDuplicate(iDic);
                        PublicGame *game = new PublicGame(*tmpGame);
                        for (int i = 0; i < _wtoi(nbHuman.c_str()); ++i)
                            game->addPlayer(new HumanPlayer);
                        for (int i = 0; i < _wtoi(nbAI.c_str()); ++i)
                            game->addPlayer(new AIPercent(1));
                        // Set the variant
                        if (tokens[0].size() > 1)
                        {
                            if (tokens[0][1] == L'j')
                                game->setVariant(PublicGame::kJOKER);
                            else if (tokens[0][1] == L'e')
                                game->setVariant(PublicGame::kEXPLOSIVE);
                        }
                        game->start();
                        loopDuplicate(*game);
                        //GameFactory::Instance()->releaseGame(*game);
                        delete game;
                    }
                    break;
                case L'l':
                    {
                        if (tokens.size() != 3)
                        {
                            help();
                            break;
                        }
                        const wstring &nbHuman = checkNumToken(tokens, 1);
                        const wstring &nbAI = checkNumToken(tokens, 2);
                        if (nbHuman == L"" || nbAI == L"")
                        {
                            help();
                            break;
                        }
                        // New free game
                        FreeGame *tmpGame = GameFactory::Instance()->createFreeGame(iDic);
                        PublicGame *game = new PublicGame(*tmpGame);
                        for (int i = 0; i < _wtoi(nbHuman.c_str()); i++)
                            game->addPlayer(new HumanPlayer);
                        for (int i = 0; i < _wtoi(nbAI.c_str()); i++)
                            game->addPlayer(new AIPercent(1));
                        // Set the variant
                        if (tokens[0].size() > 1)
                        {
                            if (tokens[0][1] == L'j')
                                game->setVariant(PublicGame::kJOKER);
                            else if (tokens[0][1] == L'e')
                                game->setVariant(PublicGame::kEXPLOSIVE);
                        }
                        game->start();
                        loopFreegame(*game);
                        //GameFactory::Instance()->releaseGame(*game);
                        delete game;
                    }
                    break;
                case L'x':
                    // Regular expression tests
                    handleRegexp(iDic, tokens);
                    break;
                case L's':
                    setSetting(tokens);
                    break;
                case L'q':
                    quit = 1;
                    break;
                default:
                    printf("commande inconnue\n");
                    break;
            }
        }
        catch (GameException &e)
        {
            printf("%s\n", e.what());
        }
    }
}


int main(int argc, char *argv[])
{
    // Let the user choose the locale
    setlocale(LC_ALL, "");

    if (argc != 2 && argc != 3)
    {
        fprintf(stdout, "Usage: eliot /chemin/vers/ods5.dawg [random_seed]\n");
        exit(1);
    }

    try
    {
        Dictionary dic(argv[1]);

        unsigned int seed;
        if (argc == 3)
            seed = atoi(argv[2]);
        else
            seed = time(NULL);
        srand(seed);
        cerr << "Using seed: " << seed << endl;

        mainLoop(dic);
        GameFactory::Destroy();

        // Free the readline static variable
        if (wline_read)
            delete[] wline_read;
    }
    catch (std::exception &e)
    {
        cerr << e.what() << endl;
    }

    return 0;
}

