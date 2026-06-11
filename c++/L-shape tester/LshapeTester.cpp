#include <iostream>
#include <windows.h>
#include <sstream>

using namespace std;

bool isUprightL(const char *arr, int xSize, int zSize)
{
    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // DRAWING BEGINS HERE --------------------------------
                            bool thisIter = true;

                            for (int bx = 0; bx < box_x && thisIter; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx; // depends on L orientation (here we have normal L)
                                    auto addressZ = j - 1 + dz; // depends on L orientation (here we have normal L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == 0)
                                            { // depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == box_x - 1)
                                            { // depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x && thisIter; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx;       // depends on L orientation (here we have normal L)
                                    auto addressZ = j - 1 + bz + d_z; // depends on L orientation (here we have normal L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || bz == box_z - 1)
                                            { // depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == box_x - 1 && bz == 0)
                                            { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_x == 0 && bx == box_x - 1)
                                            { // depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_z == 0 && bz == 0)
                                            { // depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x && thisIter; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx + box_x; // depends on L orientation (here we have normal L)
                                    auto addressZ = j - 1 + bz + d_z;   // depends on L orientation (here we have normal L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == d_x - 1 || bz == box_z - 1)
                                            { // depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bz == 0)
                                            { // depends on L orientation (here we have normal L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            if (thisIter)
                                return true;

                            // DRAWING ENDS HERE -----------------------------------------
                        }
        }

    return false;
}

bool isBackwardsUprightL(const char *arr, int xSize, int zSize)
{
    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // DRAWING BEGINS HERE --------------------------------
                            bool thisIter = true;

                            for (int bx = 0; bx < box_x && thisIter; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x; // depends on L orientation
                                    auto addressZ = j - 1 + dz;       // depends on L orientation (same as upright L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == 0)
                                            { // depends on L orientation (same as upright L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == box_x - 1)
                                            { // depends on L orientation (same as upright L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x && thisIter; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x; // depends on L orientation
                                    auto addressZ = j - 1 + bz + d_z; // depends on L orientation (same as upright L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == box_x - 1 || bz == box_z - 1)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == 0 && bz == 0)
                                            { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_x == 0 && bx == 0)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_z == 0 && bz == 0)
                                            { // depends on L orientation (same as upright L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else
                                            {
                                                if (arr[address] != '#')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x && thisIter; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx;       // depends on L orientation
                                    auto addressZ = j - 1 + bz + d_z; // depends on L orientation (same as upright L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == 0 || bz == box_z - 1)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bz == 0)
                                            { // depends on L orientation (same as upright L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else
                                            {
                                                if (arr[address] != '#')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            if (thisIter)
                                return true;

                            // DRAWING ENDS HERE -----------------------------------------
                        }
        }
    return false;
}

bool isUpsideDownL(const char *arr, int xSize, int zSize)
{
    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // DRAWING BEGINS HERE --------------------------------
                            bool thisIter = true;

                            for (int bx = 0; bx < box_x && thisIter; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x;   // depends on L orientation
                                    auto addressZ = j - 1 + dz + box_z; // depends on L orientation (same as backwards L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == d_z - 1)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == box_x - 1)
                                            { // depends on L orientation (same as backwards L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x && thisIter; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x; // depends on L orientation (same as backwards L)
                                    auto addressZ = j - 1 + bz;       // depends on L orientation
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == box_x - 1 || bz == 0)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == 0 && bz == box_z - 1)
                                            { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_x == 0 && bx == 0)
                                            { // depends on L orientation (same as backwards L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_z == 0 && bz == box_z - 1)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x && thisIter; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx; // depends on L orientation (same as backwards L)
                                    auto addressZ = j - 1 + bz; // depends on L orientation
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == 0 || bz == box_z - 1)
                                            { // depends on L orientation (same as backwards L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bz == 0)
                                            { // depends on L orientation (same as backwards L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            if (thisIter)
                                return true;

                            // DRAWING ENDS HERE -----------------------------------------
                        }
        }

    return false;
}

bool isBackwardsUpsideDownL(const char *arr, int xSize, int zSize)
{
    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // DRAWING BEGINS HERE --------------------------------
                            bool thisIter = true;

                            for (int bx = 0; bx < box_x && thisIter; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx;         // depends on L orientation
                                    auto addressZ = j - 1 + dz + box_z; // depends on L orientation (same as upside-down L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == d_z - 1)
                                            { // depends on L orientation (same as upside-down L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == box_x - 1)
                                            { // depends on L orientation (same as upside-down L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x && thisIter; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx; // depends on L orientation
                                    auto addressZ = j - 1 + bz; // depends on L orientation (same as upside-down L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || bz == 0)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bx == box_x - 1 && bz == box_z - 1)
                                            { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_x == 0 && bx == box_x - 1)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (d_z == 0 && bz == box_z - 1)
                                            { // depends on L orientation (same as upside-down L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x && thisIter; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx + box_x; // depends on L orientation
                                    auto addressZ = j - 1 + bz;         // depends on L orientation (same as upside-down L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == d_x - 1 || bz == box_z - 1)
                                            { // depends on L orientation
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (bz == 0)
                                            { // depends on L orientation (same as upside-down L)
                                                if (arr[address] != 'o')
                                                {
                                                    thisIter = false;
                                                    break;
                                                }
                                            }
                                            else if (arr[address] != '#')
                                            {
                                                thisIter = false;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            if (thisIter)
                            {
                                return true;
                            }

                            // DRAWING ENDS HERE -----------------------------------------
                        }
        }
    return false;
}

void drawUprightL();

void drawUpsideDownL();

void drawBackwardsUprightL();

void drawBackwardsUpsideDownL();

int main()
{
    drawUprightL();
}

void drawUprightL()
{
    stringstream ss;

    int xSize = 10;
    int zSize = 10;

    char arr[xSize * zSize];

    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // initialize arr.............
                            for (int k = 0; k < xSize * zSize; k++)
                            {
                                arr[k] = '-';
                            }
                            //...........................

                            for (int bx = 0; bx < box_x; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx; // depends on L orientation (here we have normal L)
                                    auto addressZ = j - 1 + dz; // depends on L orientation (here we have normal L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == 0) // depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else if (bx == box_x - 1) // depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx;       // depends on L orientation (here we have normal L)
                                    auto addressZ = j - 1 + bz + d_z; // depends on L orientation (here we have normal L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || bz == box_z - 1) // depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else if (bx == box_x - 1 && bz == 0) // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else if (d_x == 0 && bx == box_x - 1) // depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else if (d_z == 0 && bz == 0) // depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx + box_x; // depends on L orientation (here we have normal L)
                                    auto addressZ = j - 1 + bz + d_z;   // depends on L orientation (here we have normal L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == d_x - 1 || bz == box_z - 1) // depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else if (bz == 0) // depends on L orientation (here we have normal L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // print .......................
                            cout << "d_z   = " << d_z << "  d_x   = " << d_x << endl;
                            cout << "i     = " << i << "  j     = " << j << endl;
                            cout << "box_x = " << box_x << "  box_z = " << box_z << endl
                                 << endl;

                            for (int k = 0; k < zSize; k++)
                            {
                                for (int m = 0; m < xSize; m++)
                                {
                                    cout << arr[m + k * xSize] << ' ';
                                    ss << arr[m + k * xSize];
                                }
                                cout << endl;
                            }
                            cout << endl;
                            cout << "Is shape an upright L: " << isUprightL(ss.str().c_str(), xSize, zSize) << endl;
                            ss.str("");
                            cout << endl;
                            Sleep(500);
                            //..............................
                        }
        }
}

void drawBackwardsUprightL()
{
    stringstream ss;

    int xSize = 10;
    int zSize = 10;

    char arr[xSize * zSize];

    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // initialize arr.............
                            for (int k = 0; k < xSize * zSize; k++)
                            {
                                arr[k] = '-';
                            }
                            //...........................

                            for (int bx = 0; bx < box_x; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x; // depends on L orientation
                                    auto addressZ = j - 1 + dz;       // depends on L orientation (same as upright L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == 0) // depends on L orientation (same as upright L)
                                                arr[address] = 'o';
                                            else if (bx == box_x - 1) // depends on L orientation (same as upright L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x; // depends on L orientation
                                    auto addressZ = j - 1 + bz + d_z; // depends on L orientation (same as upright L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == box_x - 1 || bz == box_z - 1) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (bx == 0 && bz == 0) // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                                arr[address] = 'o';
                                            else if (d_x == 0 && bx == 0) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (d_z == 0 && bz == 0) // depends on L orientation (same as upright L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx;       // depends on L orientation
                                    auto addressZ = j - 1 + bz + d_z; // depends on L orientation (same as upright L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == 0 || bz == box_z - 1) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (bz == 0) // depends on L orientation (same as upright L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // print .......................
                            cout << "d_z   = " << d_z << "  d_x   = " << d_x << endl;
                            cout << "i     = " << i << "  j     = " << j << endl;
                            cout << "box_x = " << box_x << "  box_z = " << box_z << endl
                                 << endl;

                            for (int k = 0; k < zSize; k++)
                            {
                                for (int m = 0; m < xSize; m++)
                                {
                                    cout << arr[m + k * xSize] << ' ';
                                    ss << arr[m + k * xSize];
                                }
                                cout << endl;
                            }
                            cout << endl;
                            cout << "Is shape an backwards upright L: " << isBackwardsUprightL(ss.str().c_str(), xSize, zSize) << endl;
                            ss.str("");
                            cout << endl;
                            Sleep(500);
                            //..............................
                        }
        }
}

void drawUpsideDownL()
{
    stringstream ss;

    int xSize = 10;
    int zSize = 10;

    char arr[xSize * zSize];

    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // initialize arr.............
                            for (int k = 0; k < xSize * zSize; k++)
                            {
                                arr[k] = '-';
                            }
                            //...........................

                            for (int bx = 0; bx < box_x; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x;   // depends on L orientation
                                    auto addressZ = j - 1 + dz + box_z; // depends on L orientation (same as backwards L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == d_z - 1) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (bx == box_x - 1) // depends on L orientation (same as backwards L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx + d_x; // depends on L orientation (same as backwards L)
                                    auto addressZ = j - 1 + bz;       // depends on L orientation
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == box_x - 1 || bz == 0) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (bx == 0 && bz == box_z - 1) // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                                arr[address] = 'o';
                                            else if (d_x == 0 && bx == 0) // depends on L orientation (same as backwards L)
                                                arr[address] = 'o';
                                            else if (d_z == 0 && bz == box_z - 1) // depends on L orientation
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx; // depends on L orientation (same as backwards L)
                                    auto addressZ = j - 1 + bz; // depends on L orientation
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == 0 || bz == box_z - 1) // depends on L orientation (same as backwards L)
                                                arr[address] = 'o';
                                            else if (bz == 0) // depends on L orientation (same as backwards L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // print .......................
                            cout << "d_z   = " << d_z << "  d_x   = " << d_x << endl;
                            cout << "i     = " << i << "  j     = " << j << endl;
                            cout << "box_x = " << box_x << "  box_z = " << box_z << endl
                                 << endl;

                            for (int k = 0; k < zSize; k++)
                            {
                                for (int m = 0; m < xSize; m++)
                                {
                                    cout << arr[m + k * xSize] << ' ';
                                    ss << arr[m + k * xSize];
                                }
                                cout << endl;
                            }
                            cout << endl;
                            cout << "Is shape an upside down L: " << isUpsideDownL(ss.str().c_str(), xSize, zSize) << endl;
                            ss.str("");
                            cout << endl;
                            Sleep(500);
                            //..............................
                        }
        }
}

void drawBackwardsUpsideDownL()
{
    stringstream ss;

    int xSize = 10;
    int zSize = 10;

    char arr[xSize * zSize];

    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            for (int box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
                for (int box_x = 3; box_x <= xSize + 2 - i; box_x++)
                    for (int d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                        for (int d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
                        {
                            // initialize arr.............
                            for (int k = 0; k < xSize * zSize; k++)
                            {
                                arr[k] = '-';
                            }
                            //...........................

                            for (int bx = 0; bx < box_x; bx++) // draw vertical seg
                                for (int dz = 0; dz < d_z; dz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx;         // depends on L orientation
                                    auto addressZ = j - 1 + dz + box_z; // depends on L orientation (same as upside-down L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || dz == d_z - 1) // depends on L orientation (same as upside-down L)
                                                arr[address] = 'o';
                                            else if (bx == box_x - 1) // depends on L orientation (same as upside-down L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // draw middle box
                            for (int bx = 0; bx < box_x; bx++)
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + bx; // depends on L orientation
                                    auto addressZ = j - 1 + bz; // depends on L orientation (same as upside-down L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (bx == 0 || bz == 0) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (bx == box_x - 1 && bz == box_z - 1) // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                                arr[address] = 'o';
                                            else if (d_x == 0 && bx == box_x - 1) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (d_z == 0 && bz == box_z - 1) // depends on L orientation (same as upside-down L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            for (int dx = 0; dx < d_x; dx++) // draw horizontal seg
                                for (int bz = 0; bz < box_z; bz++)
                                {
                                    // offset so o's cross off page on start (making us start on #)
                                    auto addressX = i - 1 + dx + box_x; // depends on L orientation
                                    auto addressZ = j - 1 + bz;         // depends on L orientation (same as upside-down L)
                                    if (addressX >= 0 && addressX < xSize)
                                    {
                                        if (addressZ >= 0 && addressZ < zSize)
                                        {
                                            auto address = addressX + xSize * addressZ;
                                            if (dx == d_x - 1 || bz == box_z - 1) // depends on L orientation
                                                arr[address] = 'o';
                                            else if (bz == 0) // depends on L orientation (same as upside-down L)
                                                arr[address] = 'o';
                                            else
                                                arr[address] = '#';
                                        }
                                    }
                                    else
                                        break; // go to where bx++
                                }

                            // print .......................
                            cout << "d_z   = " << d_z << "  d_x   = " << d_x << endl;
                            cout << "i     = " << i << "  j     = " << j << endl;
                            cout << "box_x = " << box_x << "  box_z = " << box_z << endl
                                 << endl;

                            for (int k = 0; k < zSize; k++)
                            {
                                for (int m = 0; m < xSize; m++)
                                {
                                    cout << arr[m + k * xSize] << ' ';
                                    ss << arr[m + k * xSize];
                                }
                                cout << endl;
                            }
                            cout << endl;
                            cout << "Is shape a backwards upside down L: " << isBackwardsUpsideDownL(ss.str().c_str(), xSize, zSize) << endl;
                            ss.str("");
                            cout << endl;
                            Sleep(500);
                            //..............................
                        }
        }
}
