/*****************************************************************************
 * Eliot
 * Copyright (C) 2008 Olivier Teulière
 * Authors: Olivier Teulière <ipkiss @@ gmail.com>
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

#include <math.h>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

#include "board_widget.h"
#include "qtcommon.h"
#include "game.h"
#include "tile.h"
#include "board.h"

using namespace std;


const QColor BoardWidget::EmptyColour(Qt::white);
const QColor BoardWidget::L2Colour(34, 189, 240);
const QColor BoardWidget::L3Colour(29, 104, 240);
const QColor BoardWidget::W2Colour(255, 147, 196);
const QColor BoardWidget::W3Colour(240, 80, 94);
const QColor BoardWidget::TileColour(255, 235, 205);
const QColor BoardWidget::PreviewColour(183, 183, 123);


BoardWidget::BoardWidget(QWidget *parent)
    : QFrame(parent), m_game(NULL)
{
    setFrameStyle(QFrame::Panel);
    // Use as much space as possible
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setSizePolicy(policy);
    setMinimumSize(200, 200);
}


void BoardWidget::setGame(const Game *iGame)
{
    m_game = iGame;
    refresh();
}


void BoardWidget::refresh()
{
    update();
}


QSize BoardWidget::sizeHint() const
{
    return QSize(400, 400);
}


void BoardWidget::paintEvent(QPaintEvent *)
{
    int size = std::min(width(), height());
    int squareSize = (int)floor((size - 1) / (BOARD_MAX - BOARD_MIN + 2));

    // XXX: Naive implementation: we repaint everything every time
    QPainter painter(this);
    //painter.setPen(Qt::NoPen);
    for (unsigned int row = BOARD_MIN; row <= BOARD_MAX; ++row)
    {
        for (unsigned int col = BOARD_MIN; col <= BOARD_MAX; ++col)
        {
            // Set the brush color
            if (m_game != NULL && !m_game->getBoard().getTile(row, col).isEmpty())
            {
                if (m_game->getBoard().getCharAttr(row, col) & ATTR_TEST)
                    painter.setBrush(PreviewColour);
                else
                    painter.setBrush(TileColour);
            }
            else if (Board::GetWordMultiplier(row, col) == 3)
                painter.setBrush(W3Colour);
            else if (Board::GetWordMultiplier(row, col) == 2)
                painter.setBrush(W2Colour);
            else if (Board::GetLetterMultiplier(row, col) == 3)
                painter.setBrush(L3Colour);
            else if (Board::GetLetterMultiplier(row, col) == 2)
                painter.setBrush(L2Colour);
            else
                painter.setBrush(EmptyColour);
            painter.drawRect((col - BOARD_MIN + 1) * squareSize,
                             (row - BOARD_MIN + 1) * squareSize,
                             squareSize, squareSize);

            // Draw the letter
            if (m_game != NULL && !m_game->getBoard().getTile(row, col).isEmpty())
            {
                wchar_t chr = m_game->getBoard().getTile(row, col).toChar();
                if (m_game->getBoard().getCharAttr(row, col) & ATTR_JOKER)
                    chr = towlower(chr);
                painter.drawText((col - BOARD_MIN + 1) * squareSize,
                                 (row - BOARD_MIN + 1) * squareSize + 1,
                                 squareSize, squareSize,
                                 Qt::AlignCenter,
                                 qfw(wstring(1, chr)));
            }
        }
    }
    // Draw the coordinates
    for (unsigned x = 1; x <= BOARD_MAX - BOARD_MIN + 1; ++x)
    {
        painter.drawText(x * squareSize, 1,
                         squareSize, squareSize,
                         Qt::AlignCenter,
                         QString::number(x));
        painter.drawText(0, x * squareSize,
                         squareSize, squareSize,
                         Qt::AlignCenter,
                         QString(1, 'A' + x - 1));
    }
}
