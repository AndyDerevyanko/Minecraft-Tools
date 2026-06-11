#include <iostream>
#include <windows.h>
#include <sstream>

using namespace std;

bool isSquare(const char *arr, int xSize, int zSize)
{
    int sz = (xSize < zSize) ? xSize : zSize;

    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            int cap = ((i > j) ? i : j);
            for (int box = 3; box <= sz + 2 - cap; box++) // size of center box (changes over time)
            {
                // DRAWING BEGINS HERE --------------------------------
                bool thisIter = true;

                // draw middle box
                for (int bx = 0; bx < box; bx++)
                    for (int bz = 0; bz < box; bz++)
                    {
                        // offset so o's cross off page on start (making us start on #)
                        auto addressX = i - 1 + bx; // depends on L orientation (here we have normal L)
                        auto addressZ = j - 1 + bz; // depends on L orientation (here we have normal L)
                        if (addressX >= 0 && addressX < xSize)
                        {
                            if (addressZ >= 0 && addressZ < zSize)
                            {
                                auto address = addressX + xSize * addressZ;
                                if (bx == 0 || bx == box - 1)
                                { // depends on L orientation (here we have normal L)
                                    if (arr[address] != 'o')
                                    {
                                        thisIter = false;
                                        break;
                                    }
                                }
                                else if (bz == 0 || bz == box - 1)
                                { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation (here we have normal L)
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

void drawSquare();

int main()
{
    drawSquare();
}

void drawSquare()
{
    stringstream ss;

    int xSize = 10;
    int zSize = 10;

    char arr[xSize * zSize];

    int sz = (xSize < zSize) ? xSize : zSize;

    for (int i = 0; i < xSize; i++)
        for (int j = 0; j < zSize; j++)
        {
            int cap = ((i > j) ? i : j);
            for (int box = 3; box <= sz + 2 - cap; box++) // size of center box (changes over time)
            {
                // initialize arr.............
                for (int k = 0; k < xSize * zSize; k++)
                {
                    arr[k] = '-';
                }
                //...........................

                // draw middle box
                for (int bx = 0; bx < box; bx++)
                    for (int bz = 0; bz < box; bz++)
                    {
                        // offset so o's cross off page on start (making us start on #)
                        auto addressX = i - 1 + bx; // depends on L orientation (here we have normal L)
                        auto addressZ = j - 1 + bz; // depends on L orientation (here we have normal L)
                        if (addressX >= 0 && addressX < xSize)
                        {
                            if (addressZ >= 0 && addressZ < zSize)
                            {
                                auto address = addressX + xSize * addressZ;
                                if (bx == 0 || bx == box - 1) // depends on L orientation (here we have normal L)
                                    arr[address] = 'o';
                                else if (bz == 0 || bz == box - 1) // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation (here we have normal L)
                                    arr[address] = 'o';
                                else
                                    arr[address] = '#';
                            }
                        }
                        else
                            break; // go to where bx++
                    }

                // print .......................
                cout << "i     = " << i << "  j     = " << j << endl;
                cout << "box = " << box << endl
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
                cout << "Is shape a square: " << isSquare(ss.str().c_str(), xSize, zSize) << endl;
                ss.str("");
                cout << endl;
                Sleep(500);
                //..............................
            }
        }
}
