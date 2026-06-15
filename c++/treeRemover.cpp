#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <stack>
#include <thread>
#include <zlib.h>

#define TREE_TYPE_COUNT 11

// properties file works differently than others (value-key based), so we store the types of varieties we are checking for here.
std::string treeVarietyIDs[TREE_TYPE_COUNT] = {
    "oak",
    "spruce",
    "birch",
    "jungle",
    "acacia",
    "dark_oak",
    "mangrove",
    "cherry",
    "pale_oak",
    "crimson",
    "warped"};

enum xyz
{
    X_ = 0,
    Y_ = 1,
    Z_ = 2
};

struct block
{
    int x;
    int y;
    int z;
    int type;
    int prop;

    bool operator==(const block &other) const
    {
        return x == other.x &&
               y == other.y &&
               z == other.z &&
               type == other.type &&
               prop == other.prop;
    }

    friend std::ostream &operator<<(std::ostream &output, block &b)
    {
        return (output << b.type << ':' << b.prop << ':' << b.x << '.' << b.y << '.' << b.z);
    }

    // default copy constructor suffices
};

struct Layer
{
    std::vector<block> blocks;
    Layer *next;

    Layer() : next(nullptr) {}

    // all constructors by copy of vec or block, as we delete the Layer before reading in next one
    Layer(std::vector<block> b) : blocks(b), next(nullptr) {}

    Layer(block b) : next(nullptr)
    {
        blocks.push_back(b);
    }

    ~Layer()
    {
        delete next;
    }
};

struct FillerLayer : public Layer
{
    Layer *tail;

    FillerLayer(int emptyShells) : Layer()
    {

        if (emptyShells == 0)
        {
            tail = nullptr;
            return;
        }

        // allocate to this Layer
        blocks = std::vector<block>();
        Layer *p = this;

        for (int i = 0; i < emptyShells - 1; i++)
        {
            p->next = new Layer(std::vector<block>());
            p = p->next;
        }

        tail = p;
    }
};

// create "Object" that will contain all blocks within structure/feature
class Object
{
public:
    static inline int objCount = 0; // counter of all Objects
    int zenith;                     // top y coordinate (start here)
    int bottom;                     // bottom y coordinate (end here)
    Layer *head;                    // top Layer
    Layer *tail;                    // bottom Layer

    // make each child Object have its own counter
    virtual int &count()
    {
        return objCount;
    };

    // used only in specific case (createToTree function)
    Object(int z)
    {
        zenith = z;
        head = new Layer();
        tail = head;
        objCount++;
        bottom = zenith; // one element added, bottom = zenith
    }

    Object(std::vector<block> b, int z) : zenith(z)
    { // create new Object based on vector of blocks
        head = new Layer(b);
        tail = head;
        objCount++;
        bottom = zenith; // one element added, bottom = zenith
    }

    Object(block b, int z) : zenith(z)
    { // create new Object based on vector of blocks
        head = new Layer(b);
        tail = head;
        objCount++;
        bottom = zenith; // one element added, bottom = zenith
    }

    Object()
    { // empty constructor
        head = tail = nullptr;
        objCount++;
    }

    virtual ~Object()
    {
        delete head;
        objCount--;
    }

    void printObj(std::ostream &output)
    {
        auto p = head;
        while (p != nullptr)
        {
            for (auto i : p->blocks)
                output << i << std::endl;

            p = p->next;
        }
    }

    bool merge(Object *other)
    { // merge two Objects together and the ID of one of them
        // case they are equal
        if (other == this)
            return false;

        // case other is empty
        if (other->head == nullptr)
            return true;

        // case this is empty
        if (this->head == nullptr)
        {
            // make this equal to other
            this->head = other->head;
            this->tail = other->tail;

            this->zenith = other->zenith;
            this->bottom = other->bottom;

            other->bottom = other->zenith + 1; // reduce to zero length

            other->head = other->tail = nullptr;
            return true;
        }

        // cases neither are empty
        //"level" both Objects to align on zenith
        int shift = zenith - other->zenith;

        // get to Layers which align
        // if this above other, do everything as normal
        // otherwise swap the two in the process
        if (shift < 0)
        {
            // swap places
            Layer *temp = this->head;
            this->head = other->head;
            other->head = temp;

            temp = this->tail;
            this->tail = other->tail;
            other->tail = temp;

            shift = -shift;

            // swap zenithes
            int tempInt = this->zenith;
            this->zenith = other->zenith;
            other->zenith = tempInt;

            // swap bottoms
            tempInt = this->bottom;
            this->bottom = other->bottom;
            other->bottom = tempInt;
        }

        Layer *tp = head; // pointer used until we get to aligned region, goes until NON-NULL member right before aligned region

        // shift this list until we cant no moe
        for (int i = 0; i < shift; i++)
        {
            if (tp->next == nullptr)
                break;
            tp = tp->next;
        }

        // check if we do alignment logic, if no alignment present, we have the "else" part
        if (this->bottom <= other->zenith)
        { // make sure we shifted enough to align with zenith of other list

            // CASE: sum vectors of aligned Layers
            Layer *alignedtP = tp;          // guaranteed non-null as we check if tp->next not nullptr
            Layer *alignedoP = other->head; // guaranteed as Layer @ zenith of other HAS to be defined for at least 1 as we already got rid of zero-cases

            // merge these Layers and set pointers
            while (true)
            {
                // for (block& other_block : alignedoP->blocks)
                //     alignedtP->blocks.push_back(other_block);

                // idk this is supposed to be faster
                alignedtP->blocks.insert(
                    alignedtP->blocks.end(),
                    std::make_move_iterator(alignedoP->blocks.begin()),
                    std::make_move_iterator(alignedoP->blocks.end()));

                if (alignedtP != this->tail && alignedoP != other->tail)
                {
                    alignedtP = alignedtP->next;
                    alignedoP = alignedoP->next;
                }
                else
                    break;
            }

            // at end: one of or both of alignedPs point to last element processed (this makes at least one of them the last element of one list)

            // we are done merging the algined Layer blocks, one of these is a nullptr, handle accordingly:

            // case: tails of lists meet OR case: other list is shorter, do nothing
            // case: our list finishes sooner
            if (alignedtP == this->tail && alignedoP != other->tail)
            {
                alignedtP->next = alignedoP->next;
                // make other list point to the merged data ONLY, so when its deleted, we dont delete half our list
                alignedoP->next = nullptr;

                // hold current bottom to assign to other list (we chopped off past the merged part)
                int temp = this->bottom;

                // update new bottom to go all the way down, assign old bottom to other list
                this->bottom = other->bottom;
                this->tail = other->tail;
                other->bottom = temp;
                other->tail = alignedoP; // since alignedoP is the last aligned element IN other list (before chop off)
            }
            else
            {
                // no need to update this->bottom or tail
                other->head = other->tail = nullptr;
                other->bottom = other->zenith + 1; // reduce other list to zero-length
            }

            return true;
        }
        else
        { // insert filler list in between to address empty Layers, or point one list to the other
            // get amount of fillers we must make
            int numFillers = this->bottom - other->zenith - 1; // plus one because bottom is inclusive (if this->bottom + 1 == other->zenith, we need 0 fillers)

            // if there are fillers make a filler list
            if (numFillers > 0)
            {
                FillerLayer *filler = new FillerLayer(numFillers);
                // point tail of one list to heaed of filler
                tp->next = filler;

                // traverse to end of list (pFiller cannot equal nullptr)

                filler->tail->next = other->head; // point to the other list's head

                // whole filler is used so no need to delete
            }
            else
            { // no fillers
                // point this to head of other
                tp->next = other->head; // point to the other list's head
            }

            // merging complete, deallocate other, return true

            this->bottom = other->bottom; // update bottom of list to acknowledge expansion
            this->tail = other->tail;     // do same with tail

            other->head = other->tail = nullptr; // REMOVE head/tail pointer of other Object, its useless and should be deleted

            other->bottom = other->zenith + 1; // reduce other list to zero-length
            return true;
        }
    }

    Object separate(bool (*comp)(block &b))
    { // if comp == true, pull that block out into a new linked list

        // case empty
        if (this->head == nullptr)
            return *this;

        Object branch;
        branch.zenith = this->zenith;
        branch.bottom = this->bottom;

        Layer *p = head;
        Layer *q = branch.head = new Layer();

        // go block by block and move to branchblock, then assign vector to Layer of new obj
        while (true)
        {
            std::vector<block> kept;
            std::vector<block> taken;
            kept.reserve(p->blocks.size());

            // partition by moving each block once (O(m))
            for (auto &b : p->blocks)
            {
                if (comp(b))
                    taken.push_back(std::move(b));
                else
                    kept.push_back(std::move(b));
            }

            // put kept blocks back into source layer
            p->blocks = std::move(kept);

            // if anything was taken, set the branch layer's blocks
            if (!taken.empty())
                q->blocks = std::move(taken);
            else
                q->blocks.clear();

            // advance
            if (p != this->tail)
            {
                q->next = new Layer();
                q = q->next;
                p = p->next;
            }
            else
                break;
        }

        // set branch tail to its last element
        branch.tail = q;

        return branch;
    }
};

// block type ID sets for each tree-material category
std::unordered_set<int> woodTypes, logTypes, leafTypes;

// block type ID → variety index (oak=0, spruce=1, ...)
std::unordered_map<int, int> blockVariety;

// prop ID → axis (X_/Y_/Z_); only entries with an axis property appear here
std::unordered_map<int, int> propAxis;

bool isWoodOrLog(int t) { return woodTypes.count(t) || logTypes.count(t); }
bool isAnyTreeBlock(int t) { return woodTypes.count(t) || logTypes.count(t) || leafTypes.count(t); }
int getAxis(int prop) { auto it = propAxis.find(prop); return it != propAxis.end() ? it->second : -1; }
int getVariety(int type) { auto it = blockVariety.find(type); return it != blockVariety.end() ? it->second : -1; }

class Tree : public Object
{
public:
    inline static int treeCount = 0;     // Treecount specifically
    int variety;                         // oak, dark oak, acacia...
    int treeID;                          // very basic ID system, just so we have a unique number for each obj
    inline static int treeInstances = 0; // counts like treeCount but does NOT ever decrement when a tree is deleted

    int &count() override
    { // required off parent class
        return treeCount;
    }

    Tree(int z) : Object(z)
    {
        count()++;
        treeID = treeInstances++;
        variety = -1; // no variety yet
    }

    Tree(block b, int z) : Object(b, z)
    {
        count()++;
        treeID = treeInstances++;
        variety = getVariety(b.type);
    }

    Tree() : Object()
    {
        count()++;
        treeID = treeInstances++;
    }

    ~Tree()
    {
        count()--;
    }

    bool stumpCheck(block b)
    {
        if (isWoodOrLog(b.type))
            return true;
        else if (leafTypes.count(b.type))
            return false;
        else
        {
            std::cerr << "Error in leaf list/vector, non-material block selected" << std::endl;
            return false;
        }
    }
};

class Sub_Tree : public Tree // Tree but has its own counter, for checking valid trees and seeing if some components are fragmented after checks
{
public:
    inline static int sub_treeInstances = 0;
    int sub_treeID;

    Sub_Tree(int z) : Tree(z)
    {
        sub_treeID = sub_treeInstances++;
    }

    Sub_Tree(block b, int z) : Tree(b, z)
    {
        sub_treeID = sub_treeInstances++;
    }

    Sub_Tree() : Tree()
    {
        sub_treeID = sub_treeInstances++;
    }
};

// ── binary reading helpers ────────────────────────────────────────────────────

int parseAxisFromPropString(const std::string &propStr)
{
    size_t pos = propStr.find("axis=");
    if (pos == std::string::npos) return -1;
    pos += 5;
    if (pos < propStr.size()) {
        if (propStr[pos] == 'x') return X_;
        if (propStr[pos] == 'y') return Y_;
        if (propStr[pos] == 'z') return Z_;
    }
    return -1;
}

uint64_t readLE(const uint8_t *buf, int numBytes)
{
    uint64_t val = 0;
    for (int i = 0; i < numBytes; i++)
        val |= (uint64_t)(unsigned char)buf[i] << (8 * i);
    return val;
}

int decodeCoord(uint64_t raw, int numBytes)
{
    uint64_t signBit = 1ULL << (numBytes * 8 - 1);
    if (raw & signBit)
        return -(int)(raw & ~signBit);
    return (int)raw;
}

// kept only for unused legacy uses — remove if desired
std::array<int, 3> getXYZ_UNUSED(std::string line)
{
    int x = 0, y = 0, z = 0;
    bool xPos = true, yPos = true, zPos = true;
    size_t i = 0;

    // Until first ":" gives block name
    for (; line[i] != ':'; i++)
        ;
    i++;

    // until second ":" gives properties
    for (; line[i] != ':'; i++)
        ;
    i++;

    // get X
    if (line[i] == '-')
    {
        xPos = false;
        i++;
    }

    for (; line[i] != '.'; i++)
        x = x * 10 + (line[i] - '0');

    i++;

    // get Y
    if (line[i] == '-')
    {
        yPos = false;
        i++;
    }

    for (; line[i] != '.'; i++)
        y = y * 10 + (line[i] - '0');

    i++;

    // get Z

    if (line[i] == '-')
    {
        zPos = false;
        i++;
    }

    for (; i < line.length(); i++)
        z = z * 10 + (line[i] - '0');

    // done
    if (!xPos)
        x = -x;

    if (!yPos)
        y = -y;

    if (!zPos)
        z = -z;

    return {x, y, z};
}

// legacy text-format parsers — no longer called
block getBlock_UNUSED(const std::string &line)
{
    block output = {}; // create block to store output
    size_t i = 0;
    // Until first ":" gives block name
    for (; line[i] != ':'; i++)
        output.type = output.type * 10 + (line[i] - '0');
    i++;

    // until second ":" gives properties
    if (line[i] == ':')
    {
        output.prop = -1; // negative one for "no properties" ******************************************
        i++;
    }
    else
    {
        for (; line[i] != ':'; i++)
            output.prop = output.prop * 10 + (line[i] - '0');
        i++;
    }

    // parse all coordinates one by one
    auto parseCoord = [&](int &coord)
    {
        int8_t mult = 1; // multiplier depending on whether coord is negative or not

        if (line[i] == '-')
        {
            mult = -1;
            i++;
        }

        for (; line[i] != '.' && i < line.length(); i++)
            coord = coord * 10 + (line[i] - '0');

        coord = mult * coord;
        i++;
    };

    // get X,Y,Z
    parseCoord(output.x);
    parseCoord(output.y);
    parseCoord(output.z);

    // done
    return output;
}

int getX_UNUSED(const std::string &line)
{
    int output = 0;
    bool positive = true; // multiplier depending on whether coord is negative or not

    size_t i = 0;
    // Until first ":" gives block name
    for (; line[i] != ':'; i++)
        ;
    i++;

    // until second ":" gives properties
    for (; line[i] != ':'; i++)
        ;
    i++;

    if (line[i] == '-')
    {
        positive = false;
        i++;
    }

    for (; line[i] != '.'; i++)
        output = output * 10 + (line[i] - '0');

    return positive ? output : -output;
}
int getY_UNUSED(const std::string &line)
{
    int output = 0;
    bool positive = true; // multiplier depending on whether coord is negative or not

    size_t i = 0;
    // Until first ":" gives block name
    for (; line[i] != ':'; i++)
        ;
    i++;

    // until second ":" gives properties
    for (; line[i] != ':'; i++)
        ;
    i++;

    // go to first "."
    for (; line[i] != '.'; i++)
        ;
    i++;

    if (line[i] == '-')
    {
        positive = false;
        i++;
    }

    for (; line[i] != '.'; i++)
        output = output * 10 + (line[i] - '0');

    return positive ? output : -output;
}
// get z from block on line
int getZ(const std::string &line)
{
    int output = 0;
    bool positive = true; // multiplier depending on whether coord is negative or not

    size_t i = 0;
    // Until first ":" gives block name
    for (; line[i] != ':'; i++)
        ;
    i++;

    // until second ":" gives properties
    for (; line[i] != ':'; i++)
        ;
    i++;

    // go to first "."
    for (; line[i] != '.'; i++)
        ;
    i++;

    // go to second "."
    for (; line[i] != '.'; i++)
        ;
    i++;

    if (line[i] == '-')
    {
        positive = false;
        i++;
    }

    for (; i < line.length(); i++)
        output = output * 10 + (line[i] - '0');

    return positive ? output : -output;
}

int getArrIndex_Tree(std::string arr[], int size, std::string target)
{
    for (size_t i = 0; i < size; i++)
    {
        if (arr[i] == target)
        {
            return i; // found
        }
    }
    return -1; // not found
}

// tree creation (two overloads)
void createToTree(
    bool *&visitedMap,
    const int zenith,
    block **&thisLayer,
    int *&IDMap,
    int *&varietyMap,
    bool *&isWoodMap,
    bool *&isYMap,
    const int i,
    const int j,
    const int xSize,
    const int zSize,
    std::vector<Tree *> &trees);

void createToTree(
    bool *&visitedMap,
    const int zenith,
    block *&thisLayer,
    int *&IDMap,
    const int i,
    const int j,
    const int xSize,
    const int zSize,
    std::vector<Sub_Tree *> &trees);

// add surrounding blocks to tree (two overloads)
void addSurroundingToTree(
    bool *&visitedMap,
    Tree *tree,
    Layer *treeLayer,
    block **&thisLayer,
    int *&IDMap,
    int *&varietyMap,
    bool *&isWoodMap,
    bool *&isYMap,
    const int i,
    const int j,
    const int xSize,
    const int zSize);

void addSurroundingToTree(
    bool *&visitedMap,
    Sub_Tree *tree,
    Layer *treeLayer,
    block *&thisLayer,
    int *&IDMap,
    const int i,
    const int j,
    const int xSize,
    const int zSize);

// shape checks
bool isSquare(
    block *const *map,
    int i,
    int j,
    int xSize,
    int zSize);

bool isUprightL(
    block *const *map,
    block *const *nextMap,
    int i,
    int j,
    int xSize,
    int zSize,
    int &box_x,
    int &box_z,
    int &d_x,
    int &d_z);

bool isBackwardsUprightL(
    block *const *map,
    block *const *nextMap,
    int i,
    int j,
    int xSize,
    int zSize,
    int &box_x,
    int &box_z,
    int &d_x,
    int &d_z);

bool isUpsideDownL(
    block *const *map,
    block *const *nextMap,
    int i,
    int j,
    int xSize,
    int zSize,
    int &box_x,
    int &box_z,
    int &d_x,
    int &d_z);

bool isBackwardsUpsideDownL(
    block *const *map,
    block *const *nextMap,
    int i,
    int j,
    int xSize,
    int zSize,
    int &box_x,
    int &box_z,
    int &d_x,
    int &d_z);

// Push-back helpers
void push_back_uprightL(
    std::vector<std::pair<int, int>> &toVerticalDelete,
    int i,
    int j,
    int xSize,
    int zSize,
    int xMin,
    int zMin,
    int box_x,
    int box_z,
    int d_x,
    int d_z);

void push_back_backwardsUprightL(
    std::vector<std::pair<int, int>> &toVerticalDelete,
    int i,
    int j,
    int xSize,
    int zSize,
    int xMin,
    int zMin,
    int box_x,
    int box_z,
    int d_x,
    int d_z);

void push_back_upsideDownL(
    std::vector<std::pair<int, int>> &toVerticalDelete,
    int i,
    int j,
    int xSize,
    int zSize,
    int xMin,
    int zMin,
    int box_x,
    int box_z,
    int d_x,
    int d_z);

void push_back_backwardsUpsideDownL(
    std::vector<std::pair<int, int>> &toVerticalDelete,
    int i,
    int j,
    int xSize,
    int zSize,
    int xMin,
    int zMin,
    int box_x,
    int box_z,
    int d_x,
    int d_z);

int dsuFind(int elem, int *dsu) // dsu parent-find for elem
{
    int parent = elem; // find parent
    while (parent != dsu[parent])
        parent = dsu[parent];

    // path compression
    while (elem != parent)
    {
        int next = dsu[elem];
        dsu[elem] = parent;
        elem = next;
    }

    return parent;
}

// unite two dsus
void dsuUnite(int a, int b, int *dsu, int *szs)
{
    a = dsuFind(a, dsu);
    b = dsuFind(b, dsu);

    if (a == b)
        return;

    // make b always be smaller
    if (szs[a] < szs[b])
        std::swap(a, b);

    // point b to a
    dsu[b] = a;
    szs[a] += szs[b];
}

int main()
{
    // get world file name, block keylist and block propertylist
    std::string worldName;
    std::string worldBlockIDs;
    std::string worldBlockProperties;

    std::cout << "Please enter world file name + extension (ex. myworld.txt): ";
    std::cin >> worldName;
    std::cout << "Please enter world block ID list + extension (ex. myworldblockids.txt): ";
    std::cin >> worldBlockIDs;
    std::cout << "Please enter world block Properties list + extension (ex. myworldblockproperties.txt): ";
    std::cin >> worldBlockProperties;

    // open all world files (world in binary gzip because it has a text header then raw bytes)
    gzFile gz = gzopen(worldName.c_str(), "rb");
    std::ifstream blockIDs(worldBlockIDs);
    std::ifstream blockProperties(worldBlockProperties);

    if (!gz || blockIDs.fail() || blockProperties.fail())
    {
        std::cerr << "Failed to open one or more files.\n";
        if (gz) gzclose(gz);
        return 1;
    }

    // helper: read one newline-terminated line from gz stream
    auto gzGetLine = [](gzFile f) -> std::string {
        std::string s;
        int c;
        while ((c = gzgetc(f)) != -1 && c != '\n')
            s += (char)c;
        return s;
    };

    // variable to hold read file lines
    std::string line;

    // set ID values for all naturally occuring blocks
    std::unordered_set<int> naturalBlocks;

    // set ID values for all blocks that the stump of a Tree can be planted on
    std::unordered_set<int> stumpableBlocks;

    // format: "{numericID}={blockName}"
    auto endsWith = [](const std::string &s, const std::string &suffix) -> bool {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    // go thru block IDs to categorise tree blocks and build stumpable/natural sets
    while (std::getline(blockIDs, line))
    {
        size_t pos = line.find('=');
        if (pos == std::string::npos) { std::cerr << "error in blockID file" << std::endl; continue; }

        // numeric ID is before '=', block name is after '='
        int numericID = 0;
        for (size_t k = 0; k < pos; k++)
            numericID = numericID * 10 + (line[k] - '0');

        std::string blockName = line.substr(pos + 1);

        // categorise tree blocks by suffix, variety by prefix match
        auto findVariety = [&]() -> int {
            for (int v = 0; v < TREE_TYPE_COUNT; v++)
                if (blockName.substr(0, treeVarietyIDs[v].size()) == treeVarietyIDs[v])
                    return v;
            return -1;
        };

        if (endsWith(blockName, "_log")) {
            logTypes.insert(numericID);
            blockVariety[numericID] = findVariety();
        } else if (blockName == "crimson_stem" || blockName == "warped_stem") {
            logTypes.insert(numericID);
            blockVariety[numericID] = findVariety();
        } else if (endsWith(blockName, "_wood") || endsWith(blockName, "_hyphae")) {
            woodTypes.insert(numericID);
            blockVariety[numericID] = findVariety();
        } else if (endsWith(blockName, "_leaves")) {
            leafTypes.insert(numericID);
            blockVariety[numericID] = findVariety();
        }

        // set natural blockIDs in overworld
        if (
            blockName == "air" || blockName == "cave_air" || blockName == "void_air" ||
            blockName == "water" || blockName == "lava" ||
            blockName == "grass_block" || blockName == "dirt" || blockName == "coarse_dirt" || blockName == "podzol" || blockName == "mycelium" || blockName == "rooted_dirt" || blockName == "mud" || blockName == "packed_mud" ||
            blockName == "clay" || blockName == "sand" || blockName == "red_sand" || blockName == "gravel" ||
            blockName == "stone" || blockName == "granite" || blockName == "diorite" || blockName == "andesite" || blockName == "deepslate" || blockName == "tuff" || blockName == "calcite" || blockName == "bedrock" || blockName == "obsidian" || blockName == "crying_obsidian" ||
            blockName == "coal_ore" || blockName == "iron_ore" || blockName == "copper_ore" || blockName == "gold_ore" || blockName == "redstone_ore" || blockName == "lapis_ore" || blockName == "diamond_ore" || blockName == "emerald_ore" ||
            blockName == "nether_gold_ore" || blockName == "nether_quartz_ore" || blockName == "ancient_debris" ||
            isAnyTreeBlock(numericID) || endsWith(blockName, "_sapling") ||
            blockName == "plant" || blockName == "double_plant" || blockName == "dead_bush" ||
            blockName == "vine" || blockName == "glow_lichen" || blockName == "hanging_roots" || blockName == "spore_blossom" ||
            blockName == "big_dripleaf" || blockName == "small_dripleaf" || blockName == "pink_petals" ||
            blockName == "kelp" || blockName == "seagrass" || blockName == "tall_seagrass" ||
            blockName == "bamboo" || blockName == "sugar_cane" || blockName == "lily_pad" ||
            blockName == "azalea" || blockName == "flowering_azalea" ||
            blockName == "moss_block" || blockName == "moss_carpet" ||
            blockName == "mangrove_roots" || blockName == "muddy_mangrove_roots" ||
            blockName == "sculk" || blockName == "sculk_sensor" || blockName == "sculk_catalyst" || blockName == "sculk_shrieker" || blockName == "sculk_vein" ||
            blockName == "snow" || blockName == "snow_block" || blockName == "ice" || blockName == "packed_ice" || blockName == "blue_ice" || blockName == "powder_snow" ||
            blockName == "netherrack" || blockName == "soul_sand" || blockName == "soul_soil" || blockName == "glowstone" || blockName == "magma_block" ||
            blockName == "basalt" || blockName == "polished_basalt" || blockName == "blackstone" ||
            blockName == "crimson_nylium" || blockName == "warped_nylium" || blockName == "shroomlight" ||
            blockName == "nether_wart_block" || blockName == "warped_wart_block" ||
            blockName == "end_stone" || blockName == "chorus_flower" || blockName == "chorus_plant" ||
            blockName == "cobblestone" || blockName == "mossy_cobblestone" ||
            blockName == "stone_bricks" || blockName == "mossy_stone_bricks" || blockName == "cracked_stone_bricks" ||
            blockName == "infested_block" || blockName == "spawner" || blockName == "chest" || blockName == "iron_bars" ||
            endsWith(blockName, "_slab") || endsWith(blockName, "_stairs") || endsWith(blockName, "_fence") ||
            blockName == "end_portal_frame" || blockName == "trial_spawner" || blockName == "vault")
        {
            naturalBlocks.insert(numericID);
        }

        if (blockName == "dirt" || blockName == "grass_block" || blockName == "coarse_dirt" ||
            blockName == "podzol" || blockName == "rooted_dirt" || blockName == "moss_block" ||
            blockName == "pale_moss_block" || blockName == "mud" || blockName == "muddy_mangrove_roots" ||
            blockName == "mycelium" || blockName == "farmland" || blockName == "clay" ||
            blockName == "crimson_nylium" || blockName == "warped_nylium" ||
            blockName == "gravel" ||
            blockName == "sand" || blockName == "red_sand" ||
            blockName == "dirt_path" || blockName == "grass_path" ||
            blockName == "soul_sand" ||
            blockName == "snow_block")
        {
            stumpableBlocks.insert(numericID);
        }
    }

    // read binary world header: [namespace bytes: N\n] id/prop bytes: N\n x/y/z coord bytes: N\n
    auto parseHeaderLine = [](const std::string &hLine) -> int {
        size_t pos = hLine.rfind(' ');
        int val = 0;
        for (size_t k = (pos == std::string::npos ? 0 : pos + 1); k < hLine.size(); k++)
            if (hLine[k] >= '0' && hLine[k] <= '9')
                val = val * 10 + (hLine[k] - '0');
        return val;
    };

    std::string hLine = gzGetLine(gz);
    bool customWorld = (hLine.find("namespace") != std::string::npos);

    int NS_BYTES = 0, ID_PROP_BYTES, X_BYTES, Y_BYTES, Z_BYTES;
    if (customWorld) {
        NS_BYTES = parseHeaderLine(hLine);
        hLine = gzGetLine(gz); ID_PROP_BYTES = parseHeaderLine(hLine);
    } else {
        ID_PROP_BYTES = parseHeaderLine(hLine);
    }
    { std::string tmp = gzGetLine(gz); X_BYTES = parseHeaderLine(tmp); }
    { std::string tmp = gzGetLine(gz); Y_BYTES = parseHeaderLine(tmp); }
    { std::string tmp = gzGetLine(gz); Z_BYTES = parseHeaderLine(tmp); }

    z_off_t binaryStart = gztell(gz);
    int BLOCK_SIZE = (customWorld ? NS_BYTES : 0) + 2 * ID_PROP_BYTES + X_BYTES + Y_BYTES + Z_BYTES;
    int coordsOff  = (customWorld ? NS_BYTES : 0) + 2 * ID_PROP_BYTES;

    // get world height range and extents from a first binary pass
    int max = INT_MIN, min = INT_MAX;
    int xMax = INT_MIN, xMin = INT_MAX;
    int zMax = INT_MIN, zMin = INT_MAX;

    {
        std::vector<uint8_t> buf(BLOCK_SIZE);
        while (gzread(gz, buf.data(), BLOCK_SIZE) == BLOCK_SIZE)
        {
            int off = coordsOff;
            int x = decodeCoord(readLE(buf.data() + off, X_BYTES), X_BYTES); off += X_BYTES;
            int y = decodeCoord(readLE(buf.data() + off, Y_BYTES), Y_BYTES); off += Y_BYTES;
            int z = decodeCoord(readLE(buf.data() + off, Z_BYTES), Z_BYTES);
            if (y < min) min = y;
            if (y > max) max = y;
            if (x < xMin) xMin = x;
            if (x > xMax) xMax = x;
            if (z < zMin) zMin = z;
            if (z > zMax) zMax = z;
        }
    }

    // blockProperties format: "{propID}={key=val,...}"  — extract axis if present
    while (std::getline(blockProperties, line))
    {
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        int propID = 0;
        for (size_t k = 0; k < pos; k++)
            propID = propID * 10 + (line[k] - '0');
        int axis = parseAxisFromPropString(line.substr(pos + 1));
        if (axis != -1)
            propAxis[propID] = axis;
    }

    // second pass: load all relevant blocks grouped by Y level
    gzseek(gz, binaryStart, SEEK_SET);

    std::unordered_map<int, std::vector<block>> blocksByY;
    {
        std::vector<uint8_t> buf(BLOCK_SIZE);
        int typeOff = customWorld ? NS_BYTES : 0;
        while (gzread(gz, buf.data(), BLOCK_SIZE) == BLOCK_SIZE)
        {
            block b;
            b.type = (int)readLE(buf.data() + typeOff,                   ID_PROP_BYTES);
            b.prop = (int)readLE(buf.data() + typeOff + ID_PROP_BYTES,   ID_PROP_BYTES);
            int off = coordsOff;
            b.x = decodeCoord(readLE(buf.data() + off, X_BYTES), X_BYTES); off += X_BYTES;
            b.y = decodeCoord(readLE(buf.data() + off, Y_BYTES), Y_BYTES); off += Y_BYTES;
            b.z = decodeCoord(readLE(buf.data() + off, Z_BYTES), Z_BYTES);
            if (isAnyTreeBlock(b.type) || stumpableBlocks.count(b.type))
                blocksByY[b.y].push_back(b);
        }
    }

    // create vector to hold Trees
    std::vector<Tree *> trees;

    // to store relevant blocks of this Layer
    int xSize = 1 + xMax - xMin;
    int zSize = 1 + zMax - zMin;

    { // this section obtains the potential trees from the world

        // assume world file starts at max (this should always be true)
        int level = max;

        // 1D array of pointers but index using 2D logic:
        block **thisLayer = new block *[xSize * zSize];

        // allocate
        for (int i = 0; i < xSize; i++)
            for (int j = 0; j < zSize; j++)
                thisLayer[i + xSize * j] = nullptr;

        // ID MAP FOR ALL BLOCKS THAT CORRESPOND TO WHAT TREE ID, and THEIR CORRESPONDING **variety**
        int *lastLayerIDs = nullptr;       // id
        int *lastLayerVarieties = nullptr; // variety

        // BOOLEAN 2D ARRAY FOR: 1. if block is a wood/logblock 2. if blocks orientation is y (and if it even has one)
        bool *lastLayer_isWood = nullptr; // wood
        bool *lastLayer_isY = nullptr;    // y-orientation

        bool start = false; // turns on once we get past first layer

        // handle layers one by one, top to bottom
        for (; level >= min; level--)
        {
            // populate thisLayer from pre-grouped blocksByY
            {
                auto it = blocksByY.find(level);
                if (it != blocksByY.end())
                    for (const auto &b : it->second)
                        thisLayer[b.x - xMin + xSize * (b.z - zMin)] = new block(b);
            }

            // ALLOCATIONS FOR THIS LAYER (we point old layer to it)
            int *thisLayerIDs = new int[xSize * zSize];
            int *thisLayerVarieties = new int[xSize * zSize];

            bool *thisLayer_isWood = new bool[xSize * zSize];
            bool *thisLayer_isY = new bool[xSize * zSize];

            // clear ID and BOOL maps for this layer
            for (int i = 0; i < xSize; i++)
                for (int j = 0; j < zSize; j++)
                {
                    thisLayerIDs[i + xSize * j] = -1;
                    thisLayerVarieties[i + xSize * j] = -1;
                    thisLayer_isWood[i + xSize * j] = false;
                    thisLayer_isY[i + xSize * j] = false;
                }

            // once we are done collecting the current Layer, process it
            bool *visitedMap = new bool[xSize * zSize]();

            if (start)
            {
                // A: go through our current array and if any of the blocks are close enough to an ID'd block from last layer, add them to the tree with that ID
                for (int i = 0; i < xSize; i++)
                { // loop thru whole thisLayer, and see if any valid tree blocks from last layer connect (either diagonal or vertical)
                    for (int j = 0; j < zSize; j++)
                    {
                        int ID = lastLayerIDs[i + xSize * j];
                        if (ID > -1)
                        { // make sure valid ID
                            // check current layer to see if any valid blocks are below these tree elements (cross shape)
                            for (int dx = -1; dx <= 1; dx++)
                            {
                                for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
                                {
                                    int ni = i + dx; // the neighbour indices
                                    int nj = j + dz;

                                    // make sure within bounds
                                    if (ni >= 0 && ni < xSize && nj >= 0 && nj < zSize)
                                    {
                                        int addressN = ni + xSize * nj;
                                        if (!visitedMap[addressN])
                                        {
                                            // set bl to the block of this layer we are looking at

                                            block *&bl = thisLayer[addressN];

                                            if (bl == nullptr)
                                                continue;

                                            int addressT = i + xSize * j;

                                            // if this is indeed a treeBlock, we must add it to the tree of the ID
                                            bool isTreeBlock = isAnyTreeBlock(bl->type);

                                            // possible conditions for these blocks to be added to the tree of PREVIOUS layer:
                                            //  1. we are registering a tree block and its type aligns,
                                            //  OR 2. we are registering a stumpable block that is DIRECTLY below a log that is of orientation y.
                                            // Only propagate downward from y-oriented logs or leaves.
                                            // Horizontal logs must NOT cascade their tree ID down to pillars below them;
                                            // that is how building roof beams absorb vertical pillars via section A.
                                            bool prevLayerPropagates = !lastLayer_isWood[addressT] || lastLayer_isY[addressT];
                                            bool matchedTreeBlock = isTreeBlock && getVariety(bl->type) == lastLayerVarieties[addressT] && prevLayerPropagates;
                                            bool canBeStumpable = !isTreeBlock && dx == 0 && dz == 0 && lastLayer_isWood[addressT] && lastLayer_isY[addressT];

                                            if (matchedTreeBlock || canBeStumpable)
                                            {

                                                Tree *tr = trees[ID];
                                                // if we haven't yet added blocks to this tree from this layer
                                                if (tr->bottom == level + 1) // assumption: trees->tail will NOT be a nullptr, as if the tree exists, it will ONLY be created with block or vector constructor
                                                {
                                                    Layer *&p = tr->tail; // since we dont delete any trees until later, referencing vector by ID should be valid
                                                    p->next = new Layer(*bl);
                                                    p = p->next; // shift tail
                                                    tr->bottom = level;
                                                }
                                                else if (tr->bottom == level)
                                                {                                    // tree already has blocks at this level from previous loop iterations
                                                    tr->tail->blocks.push_back(*bl); // simply add block to this layer vector
                                                }
                                                else
                                                {
                                                    std::cerr << "error in tree" << std::endl;
                                                }

                                                // add to thisLayer ID map and Bool map
                                                // conditional statements in case we get stumpable block
                                                thisLayerIDs[addressN] = matchedTreeBlock ? ID : -1;
                                                thisLayerVarieties[addressN] = matchedTreeBlock ? tr->variety : -1;
                                                thisLayer_isY[addressN] = (thisLayer_isWood[addressN] = isWoodOrLog(bl->type)) ? getAxis(bl->prop) == Y_ : false;
                                                visitedMap[addressN] = true;

                                                // if not stumpable, add surrounding to tree
                                                if (!canBeStumpable) // key word: SURROUNDING: we already added the block itself to the tree, do NOT add itself again otherwise duplicate.
                                                    addSurroundingToTree(visitedMap, tr, tr->tail, thisLayer, thisLayerIDs, thisLayerVarieties, thisLayer_isWood, thisLayer_isY, i, j, xSize, zSize);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // first layer will start here, subsequent layers will do section A first

            // B: allocate all blocks that are left to a new tree, and add that tree to the ID map
            // we can guarantee ZERO stumpable blocks are left in the layer

            for (int i = 0; i < xSize; i++)
            {
                for (int j = 0; j < zSize; j++)
                {
                    // perform this RECURSIVELY
                    int address = i + xSize * j;
                    auto &bl = thisLayer[address];
                    if (bl != nullptr && isAnyTreeBlock(bl->type) && !visitedMap[address])
                        createToTree(visitedMap, level, thisLayer, thisLayerIDs, thisLayerVarieties, thisLayer_isWood, thisLayer_isY, i, j, xSize, zSize, trees);
                }
            }

            // set whole thisLayer to nullptrs not needed since we do that in CreateToTree
            // delete
            delete[] visitedMap;

            if (start)
            { // delete previous values if we have them
                delete[] lastLayerIDs;
                delete[] lastLayerVarieties;
                delete[] lastLayer_isWood;
                delete[] lastLayer_isY;

                // DELETE thisLayer
                for (int i = 0; i < xSize; i++)
                    for (int j = 0; j < zSize; j++)
                    {
                        auto address = i + xSize * j;
                        delete thisLayer[address];
                        thisLayer[address] = nullptr;
                    }
            }

            // set previous to this layer all maps
            lastLayerIDs = thisLayerIDs;
            lastLayerVarieties = thisLayerVarieties;
            lastLayer_isWood = thisLayer_isWood;
            lastLayer_isY = thisLayer_isY;

            start = true;
        }

        // delete IDMAP and Bool maps and thislayer full

        delete[] thisLayer;
        delete[] lastLayerIDs;
        delete[] lastLayerVarieties;
        delete[] lastLayer_isWood;
        delete[] lastLayer_isY;
    }

    std::cout << "Finished scanning world for trees" << std::endl;
    std::cout << "DEBUG trees after scan: " << trees.size() << std::endl;
    std::cout << "Doing merge check: " << std::endl;

    { // 1D array of pointers but index using 2D logic:
        int *IDMap_curr = new int[xSize * zSize];

        int sz = trees.size();

        // array of pointers to store all tree pointers as they walk down
        Layer **ps = new Layer *[sz];

        // allocate to nullptr
        for (int i = 0; i < sz; i++)
            ps[i] = nullptr;

        // basic union-merge
        int *dsu = new int[sz];
        // sizes for union merge
        int *szs = new int[sz];

        // allocate dsu for tree vector
        for (int i = 0; i < sz; i++)
        {
            dsu[i] = i;
            szs[i] = 1;
        }

        // go thru all layers down to up and combine conjoining trees
        for (int level = max; level >= min; level--)
        {
            // allocate currLayer
            for (int i = 0; i < xSize; i++)
                for (int j = 0; j < zSize; j++)
                    IDMap_curr[i + xSize * j] = -1;

            // go tree by tree and add to this layer
            for (int i = 0; i < sz; i++)
            {
                Tree *tree = trees[i];
                int zenith = tree->zenith;
                Layer *&t_ps = ps[i]; // set pointer to layer of each tree

                if (level == zenith) // make sure level is within this tree's bounds
                    t_ps = tree->head;

                else if (level < zenith && level >= tree->bottom)
                    t_ps = t_ps->next;

                if (t_ps != nullptr)
                    for (auto &bl : t_ps->blocks)
                        if (isAnyTreeBlock(bl.type))
                            IDMap_curr[bl.x - xMin + xSize * (bl.z - zMin)] = tree->treeID;
            }

            for (int i = 0; i < xSize; i++)
                for (int j = 0; j < zSize; j++)
                {
                    int thisID = IDMap_curr[i + xSize * j];

                    if (thisID != -1)
                    {
                        // iterate over all direct-surroundings
                        for (int dx = -1; dx <= 1; dx++)
                        {
                            for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
                            { // MAKE SURE WE DO NOT LOOP OVER THE BLOCK WE PROCESSED ABOVE AGAIN
                                if (dx == 0 && dz == 0)
                                    continue;

                                int ni = i + dx;
                                int nj = j + dz;

                                if (ni < 0 || ni >= xSize || nj < 0 || nj >= zSize)
                                    continue;

                                int otherID = IDMap_curr[ni + xSize * nj];

                                // if not part of tree, if IDs equal, OR if varieties NOT equal, skip
                                if (otherID != -1 && thisID != otherID && trees[thisID]->variety == trees[otherID]->variety)
                                {
                                    dsuUnite(thisID, otherID, dsu, szs);
                                }
                            }
                        }
                    }
                }
        }

        // merge all trees that need to be merged
        for (int i = 0; i < sz; i++)
        {
            int parent = dsuFind(i, dsu);
            if (parent != i)
            {
                Tree *&other = trees[i];
                trees[parent]->merge(other);

                // delete merged tree
                if (other != nullptr)
                {
                    delete other;
                    other = nullptr;
                }
            }
        }

        // remove all nullptr trees from our vector and shift back
        for (size_t i = 0; i < trees.size();)
        {
            if (trees[i] == nullptr)
            {
                trees[i] = trees.back();
                trees.pop_back();
            }
            else
                i++;
        }
        // delete our stuff
        delete[] IDMap_curr;
        delete[] dsu;
        delete[] szs;
        delete[] ps;
    }

    std::cout << "Finished merge check" << std::endl;
    std::cout << "DEBUG trees after DSU merge: " << trees.size() << std::endl;
    std::cout << "Filtering out invalid foilage and potential mismatches: " << std::endl;

    // to store leaves without stumps
    std::vector<Object *> foilage;

    { // filter block — Phase A (serial classify) + Phase B (parallel BFS/L-checks)

        // ── Phase A: serial classify ─────────────────────────────────────────
        // 0 = keep, 1 = foliage, 2 = discard
        std::vector<int> tStatus((int)trees.size(), 0);

        for (int ti = 0; ti < (int)trees.size(); ++ti)
        {
            Tree *tree = trees[ti];

            bool hasValidStump = false;
            for (Layer *lp = tree->head; lp != nullptr && !hasValidStump; lp = lp->next)
                for (const auto &bl : lp->blocks)
                    if (stumpableBlocks.count(bl.type)) { hasValidStump = true; break; }

            if (hasValidStump)
            {
                int lowestLogY = INT_MAX;
                for (Layer *lp = tree->head; lp != nullptr; lp = lp->next)
                    for (const auto &lb : lp->blocks)
                        if (isWoodOrLog(lb.type) && lb.y < lowestLogY)
                            lowestLogY = lb.y;

                bool stumpLayerValid = false;
                if (lowestLogY != INT_MAX)
                    for (Layer *lp = tree->head; lp != nullptr && !stumpLayerValid; lp = lp->next)
                        for (const auto &lb : lp->blocks)
                            if (lb.y == lowestLogY && isWoodOrLog(lb.type))
                                if (woodTypes.count(lb.type) || getAxis(lb.prop) == Y_)
                                    stumpLayerValid = true;

                if (!stumpLayerValid) hasValidStump = false;
            }

            if (!hasValidStump)
            {
                bool hasLeaf = false;
                for (Layer *lp = tree->head; lp != nullptr && !hasLeaf; lp = lp->next)
                    for (const auto &bl : lp->blocks)
                        if (leafTypes.count(bl.type)) { hasLeaf = true; break; }
                tStatus[ti] = hasLeaf ? 1 : 2;
            }
            else
            {
                bool isLeafless = true;
                for (Layer *lp = tree->head; lp != nullptr && isLeafless; lp = lp->next)
                    for (const auto &bl : lp->blocks)
                        if (leafTypes.count(bl.type)) { isLeafless = false; break; }
                if (isLeafless) tStatus[ti] = 2;
            }
        }

        // partition: kept trees stay in trees; foliage → foilage; discard dropped
        {
            std::vector<Tree *> kept;
            kept.reserve(trees.size());
            for (int i = 0; i < (int)trees.size(); ++i)
            {
                if      (tStatus[i] == 1) foilage.push_back(trees[i]);
                else if (tStatus[i] == 0) kept.push_back(trees[i]);
                // tStatus[i] == 2 → discard (same as original: pointer dropped)
            }
            trees = std::move(kept);
        }

        // ── Phase B: BFS + L-checks + stumpable removal (parallel) ──────────
        {
            int n = (int)trees.size();
            unsigned int nT = std::max(1u, std::thread::hardware_concurrency());

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
            // Each thread handles a disjoint range of tree indices.
            // All per-tree state (xMin/xMax/etc.) is declared locally inside the
            // lambda so there are no shared writes and no race conditions.
            auto doWork = [&](int start, int end)
            {
                for (int ti = start; ti < end; ++ti)
                {
                    Tree *tree = trees[ti];
                    Layer *p;

                    // ── log-connectivity BFS ─────────────────────────────────
                    {
                        auto logPack = [](int x, int y, int z) -> int64_t {
                            return ((int64_t)(x + (1 << 25)) & 0x3FFFFFF)
                                 | (((int64_t)(z + (1 << 25)) & 0x3FFFFFF) << 26)
                                 | (((int64_t)(y + 64) & 0xFFF) << 52);
                        };

                        std::unordered_set<int64_t> allLogs;
                        std::unordered_map<int64_t, int> logAxis;
                        for (Layer *lp = tree->head; lp != nullptr; lp = lp->next)
                            for (const auto &lb : lp->blocks)
                                if (isWoodOrLog(lb.type)) {
                                    int64_t k = logPack(lb.x, lb.y, lb.z);
                                    allLogs.insert(k);
                                    int ax = woodTypes.count(lb.type) ? Y_ : getAxis(lb.prop);
                                    logAxis[k] = (ax == -1) ? Y_ : ax;
                                }

                        std::unordered_set<int64_t> reachLogs;
                        std::stack<std::array<int,3>> bfsStk;
                        for (Layer *lp = tree->head; lp != nullptr; lp = lp->next)
                            for (const auto &lb : lp->blocks)
                                if (isWoodOrLog(lb.type) && (woodTypes.count(lb.type) || getAxis(lb.prop) == Y_)) {
                                    int64_t k = logPack(lb.x, lb.y, lb.z);
                                    if (reachLogs.insert(k).second)
                                        bfsStk.push({lb.x, lb.y, lb.z});
                                }

                        const int dirs6[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
                        while (!bfsStk.empty()) {
                            std::array<int,3> cur = bfsStk.top(); bfsStk.pop();
                            int64_t curKey = logPack(cur[0], cur[1], cur[2]);
                            bool hLog = (logAxis[curKey] != Y_);
                            for (const auto &d : dirs6) {
                                if (hLog && d[1] != 0) continue;
                                int nx = cur[0]+d[0], ny = cur[1]+d[1], nz = cur[2]+d[2];
                                int64_t nk = logPack(nx, ny, nz);
                                if (allLogs.count(nk) && reachLogs.insert(nk).second)
                                    bfsStk.push({nx, ny, nz});
                            }
                        }

                        for (Layer *lp = tree->head; lp != nullptr; lp = lp->next) {
                            auto &blks = lp->blocks;
                            for (size_t bi = 0; bi < blks.size(); ) {
                                if (isWoodOrLog(blks[bi].type) &&
                                    !reachLogs.count(logPack(blks[bi].x, blks[bi].y, blks[bi].z)))
                                    { blks[bi] = blks.back(); blks.pop_back(); }
                                else ++bi;
                            }
                        }
                    }

                    // ── L-shape & axis checks ────────────────────────────────
                    {
                        // local min/max/size — each thread stack frame owns these,
                        // so no writes race with the outer-scope variables of the same name
                        int xMin = INT_MAX, zMin = INT_MAX;
                        int xMax = INT_MIN, zMax = INT_MIN;

                        p = tree->head;
                        while (p != nullptr)
                        {
                            for (auto &bl : p->blocks)
                            {
                                int x = bl.x;
                                int z = bl.z;
                                if (z < zMin) zMin = z;
                                if (z > zMax) zMax = z;
                                if (x < xMin) xMin = x;
                                if (x > xMax) xMax = x;
                            }
                            p = p->next;
                        }

                        int xSize = 1 + xMax - xMin;
                        int zSize = 1 + zMax - zMin;

                        std::vector<std::pair<int, int>> toVerticalDelete;

                        block **blockMapTop    = nullptr;
                        block **blockMapBottom = nullptr;
                        block **blockMap       = nullptr;

                        Layer *topL    = nullptr;
                        p              = nullptr;
                        Layer *bottomL = tree->head;

                        int sz = xSize * zSize;

                        while (true)
                        {
                            delete[] blockMapTop;
                            blockMapTop   = blockMap;
                            blockMap      = blockMapBottom;

                            blockMapBottom = new block *[sz];
                            for (int i = 0; i < sz; i++) blockMapBottom[i] = nullptr;

                            if (bottomL != nullptr)
                            {
                                auto &blks = bottomL->blocks;
                                for (auto &bl : blks)
                                    blockMapBottom[bl.x - xMin + xSize * (bl.z - zMin)] = &bl;
                            }

                            if (p != nullptr)
                            {
                                for (int i = 0; i < xSize; i++)
                                    for (int j = 0; j < zSize;)
                                    {
                                        auto &bl = blockMap[i + xSize * j];

                                        if (bl == nullptr) { j++; continue; }

                                        auto type = bl->type;

                                        if (!isWoodOrLog(type)) { j++; continue; }

                                        // 1. L-shape check
                                        {
                                            int box_x, box_z, d_x, d_z;

                                            if (isUprightL(blockMap, blockMapBottom, i, j, xSize, zSize, box_x, box_z, d_x, d_z))
                                            {
                                                if (!isSquare(blockMap, i, j, xSize, zSize))
                                                {
                                                    push_back_uprightL(toVerticalDelete, i, j, xSize, zSize, xMin, zMin, box_x, box_z, d_x, d_z);
                                                    j++; continue;
                                                }
                                            }
                                            else if (isBackwardsUprightL(blockMap, blockMapBottom, i, j, xSize, zSize, box_x, box_z, d_x, d_z))
                                            {
                                                if (!isSquare(blockMap, i, j, xSize, zSize))
                                                {
                                                    push_back_backwardsUprightL(toVerticalDelete, i, j, xSize, zSize, xMin, zMin, box_x, box_z, d_x, d_z);
                                                    j++; continue;
                                                }
                                            }
                                            else if (isUpsideDownL(blockMap, blockMapBottom, i, j, xSize, zSize, box_x, box_z, d_x, d_z))
                                            {
                                                if (!isSquare(blockMap, i, j, xSize, zSize))
                                                {
                                                    push_back_upsideDownL(toVerticalDelete, i, j, xSize, zSize, xMin, zMin, box_x, box_z, d_x, d_z);
                                                    j++; continue;
                                                }
                                            }
                                            else if (isBackwardsUpsideDownL(blockMap, blockMapBottom, i, j, xSize, zSize, box_x, box_z, d_x, d_z))
                                            {
                                                if (!isSquare(blockMap, i, j, xSize, zSize))
                                                {
                                                    push_back_backwardsUpsideDownL(toVerticalDelete, i, j, xSize, zSize, xMin, zMin, box_x, box_z, d_x, d_z);
                                                    j++; continue;
                                                }
                                            }
                                        }

                                        // 2. axis check
                                        int thisAxis = getAxis(bl->prop);

                                        bool isValid = false;
                                        int otherAxis;
                                        block *otherBlock;

                                        if (thisAxis == X_)
                                        {
                                            if (i - 1 >= 0)
                                            {
                                                otherBlock = blockMap[i - 1 + xSize * j];
                                                if (otherBlock != nullptr)
                                                {
                                                    otherAxis = getAxis(otherBlock->prop);
                                                    if (otherAxis == Y_ || otherAxis == Z_) { j++; continue; }
                                                }
                                            }
                                            if (i + 1 < xSize)
                                            {
                                                otherBlock = blockMap[i + 1 + xSize * j];
                                                if (otherBlock != nullptr)
                                                {
                                                    otherAxis = getAxis(otherBlock->prop);
                                                    if (otherAxis == Y_ || otherAxis == Z_) { j++; continue; }
                                                }
                                            }
                                        }
                                        else if (thisAxis == Z_)
                                        {
                                            if (j - 1 >= 0)
                                            {
                                                otherBlock = blockMap[i + xSize * (j - 1)];
                                                if (otherBlock != nullptr)
                                                {
                                                    otherAxis = getAxis(otherBlock->prop);
                                                    if (otherAxis == Y_ || otherAxis == X_) { j++; continue; }
                                                }
                                            }
                                            if (j + 1 < zSize)
                                            {
                                                otherBlock = blockMap[i + xSize * (j + 1)];
                                                if (otherBlock != nullptr)
                                                {
                                                    otherAxis = getAxis(otherBlock->prop);
                                                    if (otherAxis == Y_ || otherAxis == X_) { j++; continue; }
                                                }
                                            }
                                        }
                                        else
                                        { // Y axis — always valid
                                            j++; continue;
                                        }

                                        for (int dx = -1; dx <= 1 && !isValid; dx++)
                                        {
                                            for (int dy = -1; dy <= 1; dy++)
                                            {
                                                if (dy == 1 && topL == nullptr) continue;
                                                if (dy == -1 && bottomL == nullptr) continue;

                                                for (int dz = (dy == 0 ? -1 : (dx == 0 ? -1 : 0)); dz <= (dy == 0 ? 1 : (dx == 0 ? 1 : 0)); dz++)
                                                {
                                                    if (dx == 0 && dz == 0 && dy == 0) continue;

                                                    int ni = i + dx;
                                                    int nj = j + dz;
                                                    auto address = ni + xSize * nj;

                                                    if (ni < 0 || ni >= xSize || nj < 0 || nj >= zSize) continue;

                                                    if (dy == 0)
                                                    {
                                                        otherBlock = blockMap[address];
                                                        if (otherBlock != nullptr && getAxis(otherBlock->prop) == -1)
                                                        { isValid = true; j++; break; }
                                                    }
                                                    else if (dy == -1)
                                                    {
                                                        otherBlock = blockMap[address];
                                                        if (otherBlock != nullptr && getAxis(otherBlock->prop) == -1)
                                                        { isValid = true; j++; break; }
                                                    }
                                                    else if (dy == 1)
                                                    {
                                                        otherBlock = blockMap[address];
                                                        if (otherBlock != nullptr && getAxis(otherBlock->prop) == -1)
                                                        { isValid = true; j++; break; }
                                                    }
                                                }
                                            }
                                        }

                                        if (!isValid)
                                        {
                                            bl->prop = -2;
                                            j++;
                                        }
                                    }
                            }

                            topL = p;
                            p = bottomL;
                            if (bottomL != nullptr) bottomL = bottomL->next;
                            else break;
                        }

                        delete[] blockMapBottom;
                        delete[] blockMapTop;
                        delete[] blockMap;

                        p = tree->head;
                        blockMap = new block *[sz];

                        while (p != nullptr)
                        {
                            for (int i = 0; i < sz; i++) blockMap[i] = nullptr;

                            for (auto &bl : p->blocks)
                                blockMap[bl.x - xMin + xSize * (bl.z - zMin)] = &bl;

                            for (auto idx : toVerticalDelete)
                            {
                                auto &blk = blockMap[idx.first + xSize * idx.second];
                                if (blk != nullptr) blk->prop = -2;
                            }

                            auto &blks = p->blocks;
                            for (int i = 0; i < (int)blks.size();)
                            {
                                if (blks[i].prop == -2) { blks[i] = blks.back(); blks.pop_back(); }
                                else ++i;
                            }

                            p = p->next;
                        }

                        delete[] blockMap;
                    }

                    // ── remove stumpable blocks ──────────────────────────────
                    p = tree->head;
                    while (p != nullptr)
                    {
                        auto &blks = p->blocks;
                        for (int i = 0; i < (int)blks.size();)
                        {
                            if (!isAnyTreeBlock(blks[i].type)) { blks[i] = blks.back(); blks.pop_back(); }
                            else ++i;
                        }
                        p = p->next;
                    }
                }
            };
#pragma GCC diagnostic pop

            if (n > 0)
            {
                int chunk = std::max(1, n / (int)nT);
                std::vector<std::thread> threads;
                for (unsigned int t = 0; t < nT; ++t)
                {
                    int s = t * chunk;
                    int e = (t + 1 == nT) ? n : s + chunk;
                    if (s < e) threads.emplace_back(doWork, s, e);
                }
                for (auto &th : threads) th.join();
            }
        }
    }

    std::cout << "Finished filtering out invalid foilage and potential mismatches" << std::endl;
    std::cout << "DEBUG trees after filter: " << trees.size() << "  foilage: " << foilage.size() << std::endl;
    std::cout << "Getting rid of orphan blocks: " << std::endl;

    std::vector<Tree *> treesToAdd;

    // 3: Orphan check: if a tree is only pieces of wood and separate from main body of main tree, we delete it
    {
        for (auto &head_tree : trees)
        {
            std::vector<Sub_Tree *> sub_trees;
            // IMPORTANT: clear out instances, so sub_IDs get proper referencing
            Sub_Tree::sub_treeInstances = 0;

            int level = head_tree->zenith;

            // get max/mins for map dimensions
            xMin = INT_MAX, zMin = INT_MAX;
            xMax = INT_MIN, zMax = INT_MIN;

            Layer *p = head_tree->head;

            while (p != nullptr)
            {
                for (auto &bl : p->blocks)
                {
                    int x = bl.x;
                    int z = bl.z;

                    if (z < zMin)
                        zMin = z;

                    if (z > zMax)
                        zMax = z;

                    if (x < xMin)
                        xMin = x;

                    if (x > xMax)
                        xMax = x;
                }
                p = p->next;
            }

            xSize = 1 + xMax - xMin;
            zSize = 1 + zMax - zMin;

            // 1D array of pointers but index using 2D logic:
            block *thisLayer = new block[xSize * zSize];

            // allocate
            for (int i = 0; i < xSize; i++)
                for (int j = 0; j < zSize; j++)
                {
                    thisLayer[i + xSize * j].type = -1;
                }

            int *lastLayerIDs = nullptr;

            // start search at head
            p = head_tree->head;

            while (p != nullptr)
            { // handle layers one by one

                for (auto &bl : p->blocks)
                    thisLayer[bl.x - xMin + xSize * (bl.z - zMin)] = bl;

                // ALLOCATIONS FOR THIS LAYER (we point old layer to it)
                int *thisLayerIDs = new int[xSize * zSize];

                // clear map for this layer
                for (int i = 0; i < xSize; i++)
                    for (int j = 0; j < zSize; j++)
                        thisLayerIDs[i + xSize * j] = -1;

                // once we are done collecting the current Layer, process it
                bool *visitedMap = new bool[xSize * zSize]();

                if (lastLayerIDs != nullptr)
                {
                    // A: go through our current array and if any of the blocks are close enough to an ID'd block from last layer, add them to the tree with that ID
                    for (int i = 0; i < xSize; i++)
                    { // loop thru whole thisLayer, and see if any valid tree blocks from last layer connect (either diagonal or vertical)
                        for (int j = 0; j < zSize; j++)
                        {
                            int sub_ID = lastLayerIDs[i + xSize * j];
                            if (sub_ID > -1)
                            { // make sure valid ID
                                // check current layer to see if any valid blocks are below these tree elements (cross shape)
                                for (int dx = -1; dx <= 1; dx++)
                                {
                                    for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
                                    {
                                        int ni = i + dx; // the neighbour indices
                                        int nj = j + dz;

                                        // make sure within bounds
                                        if (ni >= 0 && ni < xSize && nj >= 0 && nj < zSize)
                                        {
                                            int addressN = ni + xSize * nj;
                                            if (!visitedMap[addressN])
                                            {
                                                // set bl to the block of this layer we are looking at

                                                block &bl = thisLayer[addressN];

                                                if (bl.type == -1)
                                                    continue;

                                                Sub_Tree *sub_tr = sub_trees[sub_ID];

                                                // if we haven't yet added blocks to this tree from this layer
                                                if (sub_tr->bottom == level + 1)
                                                {
                                                    Layer *&q = sub_tr->tail; // since we dont delete any trees until later, referencing vector by ID should be valid
                                                    q->next = new Layer(bl);
                                                    q = q->next; // shift tail
                                                    sub_tr->bottom = level;
                                                }
                                                else if (sub_tr->bottom == level)
                                                {                                       // tree already has blocks at this level from previous loop iterations
                                                    sub_tr->tail->blocks.push_back(bl); // simply add block to this layer vector
                                                }
                                                else
                                                {
                                                    std::cerr << "error in tree " << std::endl;
                                                }

                                                // add to thisLayer ID map and Bool map
                                                // conditional statements in case we get stumpable block
                                                thisLayerIDs[addressN] = sub_ID;
                                                visitedMap[addressN] = true;

                                                // Add surrounding to tree
                                                addSurroundingToTree(visitedMap, sub_tr, sub_tr->tail, thisLayer, thisLayerIDs, i, j, xSize, zSize);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // first layer will start here, subsequent layers will do section A first

                // B: allocate all blocks that are left to a new tree, and add that tree to the ID map

                for (int i = 0; i < xSize; i++)
                {
                    for (int j = 0; j < zSize; j++)
                    {
                        // perform this RECURSIVELY
                        int address = i + xSize * j;
                        auto bl = thisLayer[address];
                        if (bl.type != -1 && !visitedMap[address])
                            createToTree(visitedMap, level, thisLayer, thisLayerIDs, i, j, xSize, zSize, sub_trees);
                    }
                }

                // set whole thisLayer to nullptrs not needed since we do that in CreateToTree
                if (lastLayerIDs != nullptr)
                {
                    delete[] lastLayerIDs;
                    delete[] visitedMap;

                    // clear thisLayer
                    for (int i = 0; i < xSize; i++)
                        for (int j = 0; j < zSize; j++)
                        {
                            thisLayer[i + xSize * j].type = -1;
                        }
                }

                // set previous to this layer all maps
                lastLayerIDs = thisLayerIDs;

                p = p->next;
                level--;
            }

            // delete this layer
            delete[] thisLayer;

            // 1D array of pointers but index using 2D logic:
            int *IDMap_curr = new int[xSize * zSize];

            int sz = sub_trees.size();

            // array of pointers to store all tree pointers as they walk down
            Layer **ps = new Layer *[sz];

            // allocate to nullptr
            for (int i = 0; i < sz; i++)
                ps[i] = nullptr;

            // basic union-merge
            int *dsu = new int[sz];
            // sizes for union merge
            int *szs = new int[sz];

            // allocate dsu for tree vector
            for (int i = 0; i < sz; i++)
            {
                dsu[i] = i;
                szs[i] = 1;
            }

            // go thru all layers down to up and combine conjoining trees
            // note: we reuse level variable
            for (level = head_tree->zenith; level >= head_tree->bottom; level--)
            {
                // allocate currLayer
                for (int i = 0; i < xSize; i++)
                    for (int j = 0; j < zSize; j++)
                        IDMap_curr[i + xSize * j] = -1;

                // go tree by tree and add to this layer
                for (int i = 0; i < sz; i++)
                {
                    Sub_Tree *sub_tree = sub_trees[i];
                    int zenith = sub_tree->zenith;
                    Layer *&t_ps = ps[i];

                    if (level == zenith) // make sure level is within this tree's bounds
                        t_ps = sub_tree->head;

                    else if (level < zenith && level >= sub_tree->bottom)
                        t_ps = t_ps->next;

                    if (t_ps != nullptr)
                        for (auto &bl : t_ps->blocks)
                            if (isAnyTreeBlock(bl.type))
                                IDMap_curr[bl.x - xMin + xSize * (bl.z - zMin)] = sub_tree->sub_treeID;
                }

                for (int i = 0; i < xSize; i++)
                    for (int j = 0; j < zSize; j++)
                    {
                        int thisID = IDMap_curr[i + xSize * j];

                        if (thisID != -1)
                        {
                            // iterate over all direct-surroundings
                            for (int dx = -1; dx <= 1; dx++)
                            {
                                for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
                                { // MAKE SURE WE DO NOT LOOP OVER THE BLOCK WE PROCESSED ABOVE AGAIN
                                    if (dx == 0 && dz == 0)
                                        continue;

                                    int ni = i + dx;
                                    int nj = j + dz;

                                    if (ni < 0 || ni >= xSize || nj < 0 || nj >= zSize)
                                        continue;

                                    int otherID = IDMap_curr[ni + xSize * nj];

                                    // if not part of tree or IDs equal, skip
                                    if (otherID != -1 && thisID != otherID)
                                    {
                                        dsuUnite(thisID, otherID, dsu, szs);
                                    }
                                }
                            }
                        }
                    }
            }

            // merge all trees that need to be merged
            for (int i = 0; i < sz; i++)
            {
                int parent = dsuFind(i, dsu);
                if (parent != i)
                {
                    Sub_Tree *&other = sub_trees[i];
                    sub_trees[parent]->merge(other);

                    // delete merged tree
                    if (other != nullptr)
                    {
                        delete other;
                        other = nullptr;
                    }
                }
            }

            // cant use sz as size is no longer constant
            //  remove all nullptr trees from our vector and shift back
            for (auto i = 0; i < sub_trees.size();)
            {
                if (sub_trees[i] == nullptr)
                {

                    sub_trees[i] = sub_trees.back();
                    sub_trees.pop_back();
                }
                else
                    i++;
            }

            // delete our stuff
            delete[] IDMap_curr;
            delete[] dsu;
            delete[] szs;
            delete[] ps;

            // if subtrees has only one tree, we know that tree = subtree meaning no additional work needs to be done, otherwise do the extra stuff
            if (sub_trees.size() > 1)
            {
                // tree will only be empty in the event it consisits of wood purely
                for (auto i = 0; i < sub_trees.size();)
                {
                    bool purelyWood = true;
                    // bool purelyLeaves = true; NOT needed as this should be impossible after doing our previous checks

                    auto &thisTree = sub_trees[i];

                    // reuse p
                    p = thisTree->head;

                    while (p != nullptr && purelyWood)
                    {
                        for (auto &bl : p->blocks)
                        {
                            if (leafTypes.count(bl.type))
                            {
                                purelyWood = false;
                                break;
                            }
                        }

                        p = p->next;
                    }

                    if (purelyWood)
                    {
                        delete thisTree;
                        thisTree = sub_trees.back();
                        sub_trees.pop_back();
                    }
                    else
                        i++;
                }

                sz = sub_trees.size(); // update sz

                // DEBUGGING:
                if (sz == 0)
                {
                    std::cout << "Error in tree function: tree check has removed all trees within" << std::endl;
                }
                //.....

                // if more than one tree still left, we add them as separate tree objects
                if (sz > 1)
                {
                    for (auto i = 1; i < sub_trees.size(); i++)
                        treesToAdd.push_back(sub_trees[i]);
                }

                // make overall tree vector point to new pruned tree
                delete head_tree;
                head_tree = sub_trees[0];
            }
        }
    }

    // DEBUGGING: check if any empty trees exist

    // add subtrees to trees
    trees.reserve(trees.size() + treesToAdd.size());
    trees.insert(
        trees.end(),
        std::make_move_iterator(treesToAdd.begin()),
        std::make_move_iterator(treesToAdd.end()));

    // DEBUGGING: PRINT ALL TREES
    std::ofstream debugOut("debugOut.txt");
    for (auto tree : trees)
        tree->printObj(debugOut);

    // derive output stem from worldName (strip extension)
    std::string outStem = worldName;
    {
        auto dot = outStem.rfind('.');
        if (dot != std::string::npos)
            outStem = outStem.substr(0, dot);
    }

    auto packXYZ = [](int x, int y, int z) -> int64_t {
        return ((int64_t)(x & 0x3FFFFFF))
             | ((int64_t)(z & 0x3FFFFFF) << 26)
             | ((int64_t)(y & 0xFFF)     << 52);
    };

    // ── Iteratively attach orphan foliage to adjacent confirmed trees ─────
    // Handles custom trees whose leaf variety doesn't match their trunk type.
    {
        std::unordered_set<int64_t> confirmedSet;
        for (auto tree : trees) {
            Layer *p = tree->head;
            while (p != nullptr) {
                for (const auto &bl : p->blocks)
                    confirmedSet.insert(packXYZ(bl.x, bl.y, bl.z));
                p = p->next;
            }
        }

        bool any_merged = true;
        while (any_merged) {
            any_merged = false;
            for (size_t fi = 0; fi < foilage.size();) {
                Object *obj = foilage[fi];
                bool attached = false;
                for (Layer *p = obj->head; p != nullptr && !attached; p = p->next) {
                    for (const auto &bl : p->blocks) {
                        for (int dx = -1; dx <= 1 && !attached; dx++)
                            for (int dy = -1; dy <= 1 && !attached; dy++)
                                for (int dz = -1; dz <= 1 && !attached; dz++) {
                                    if (dx == 0 && dy == 0 && dz == 0) continue;
                                    if (confirmedSet.count(packXYZ(bl.x+dx, bl.y+dy, bl.z+dz)))
                                        attached = true;
                                }
                        if (attached) break;
                    }
                }
                if (attached) {
                    for (Layer *p = foilage[fi]->head; p != nullptr; p = p->next)
                        for (const auto &bl : p->blocks)
                            confirmedSet.insert(packXYZ(bl.x, bl.y, bl.z));
                    trees.push_back(static_cast<Tree*>(foilage[fi]));
                    foilage[fi] = foilage.back();
                    foilage.pop_back();
                    any_merged = true;
                } else {
                    fi++;
                }
            }
        }
    }

    // ── build lookup sets from original world data (blocksByY is unmodified) ─
    std::unordered_set<int64_t> occupiedXYZ, stumpableXYZ;
    for (auto &[y, blks] : blocksByY)
        for (const auto &b : blks) {
            occupiedXYZ.insert(packXYZ(b.x, b.y, b.z));
            if (stumpableBlocks.count(b.type))
                stumpableXYZ.insert(packXYZ(b.x, b.y, b.z));
        }

    // ── find stump block for each confirmed tree ──────────────────────────
    // Priority: Y-log above stumpable (1) > wood above stumpable (2) >
    //           Y-log above any solid (3) > wood above any solid (4)
    std::unordered_set<int64_t> stumpCoords;
    for (auto *treePtr : trees) {
        int lowestLogY = INT_MAX;
        for (Layer *lp = treePtr->head; lp != nullptr; lp = lp->next)
            for (const auto &bl : lp->blocks)
                if (isWoodOrLog(bl.type) && bl.y < lowestLogY)
                    lowestLogY = bl.y;

        if (lowestLogY == INT_MAX) continue;

        const block *bestBlock = nullptr;
        int bestPri = 6;

        for (Layer *lp = treePtr->head; lp != nullptr; lp = lp->next)
            for (const auto &bl : lp->blocks)
                if (bl.y == lowestLogY && isWoodOrLog(bl.type))
                {
                    int64_t below = packXYZ(bl.x, lowestLogY - 1, bl.z);
                    bool abvStump = stumpableXYZ.count(below) > 0;
                    bool abvSolid = occupiedXYZ.count(below) > 0;
                    bool isYLog   = !woodTypes.count(bl.type) && getAxis(bl.prop) == Y_;
                    bool isWoodBl = woodTypes.count(bl.type) > 0;

                    int pri;
                    if      (isYLog   && abvStump) pri = 1;
                    else if (isWoodBl && abvStump) pri = 2;
                    else if (isYLog   && abvSolid) pri = 3;
                    else if (isWoodBl && abvSolid) pri = 4;
                    else                           pri = 5;

                    if (pri < bestPri) { bestPri = pri; bestBlock = &bl; }
                }

        if (bestBlock != nullptr)
            stumpCoords.insert(packXYZ(bestBlock->x, bestBlock->y, bestBlock->z));
    }

    // ── foliage coordinate set ────────────────────────────────────────────
    std::unordered_set<int64_t> foliageCoords;
    for (auto obj : foilage) {
        Layer *p = obj->head;
        while (p != nullptr) {
            for (const auto &bl : p->blocks) foliageCoords.insert(packXYZ(bl.x, bl.y, bl.z));
            p = p->next;
        }
    }

    // ── find max existing block ID, assign marker IDs ─────────────────────
    int maxBlockID = 0;
    {
        blockIDs.clear();
        blockIDs.seekg(0);
        std::string bline;
        while (std::getline(blockIDs, bline)) {
            size_t pos = bline.find('=');
            if (pos == std::string::npos) continue;
            int id = 0;
            for (size_t k = 0; k < pos; k++) id = id * 10 + (bline[k] - '0');
            if (id > maxBlockID) maxBlockID = id;
        }
    }
    int treeMarkerID    = maxBlockID + 1;
    int foliageMarkerID = maxBlockID + 2;

    // bump byte width if new IDs no longer fit
    int outIDBytes = ID_PROP_BYTES;
    while (foliageMarkerID >= (1 << (outIDBytes * 8))) outIDBytes++;

    // ── write full world with marker substitution ─────────────────────────
    {
        auto wLE = [](gzFile f, uint64_t val, int n) {
            uint8_t buf[8];
            for (int i = 0; i < n; i++) buf[i] = (uint8_t)((val >> (8 * i)) & 0xFF);
            gzwrite(f, buf, (unsigned)n);
        };
        auto enc = [](int val, int n) -> uint64_t {
            return val < 0 ? ((uint64_t)(-val) | (1ULL << (n * 8 - 1))) : (uint64_t)val;
        };

        gzFile outGz = gzopen((outStem + "_trees.world").c_str(), "wb");
        if (customWorld) gzputs(outGz, ("namespace bytes: " + std::to_string(NS_BYTES) + "\n").c_str());
        gzputs(outGz, ("id/prop bytes: " + std::to_string(outIDBytes) + "\n").c_str());
        gzputs(outGz, ("x coord bytes: " + std::to_string(X_BYTES)    + "\n").c_str());
        gzputs(outGz, ("y coord bytes: " + std::to_string(Y_BYTES)    + "\n").c_str());
        gzputs(outGz, ("z coord bytes: " + std::to_string(Z_BYTES)    + "\n").c_str());

        int inTypeOff   = customWorld ? NS_BYTES : 0;
        int inPropOff   = inTypeOff + ID_PROP_BYTES;
        int inCoordsOff = inPropOff + ID_PROP_BYTES;
        int inBlockSize = inCoordsOff + X_BYTES + Y_BYTES + Z_BYTES;

        gzseek(gz, binaryStart, SEEK_SET);
        std::vector<uint8_t> buf(inBlockSize);

        while (gzread(gz, buf.data(), inBlockSize) == inBlockSize) {
            int bx = decodeCoord(readLE(buf.data() + inCoordsOff,                      X_BYTES), X_BYTES);
            int by = decodeCoord(readLE(buf.data() + inCoordsOff + X_BYTES,             Y_BYTES), Y_BYTES);
            int bz = decodeCoord(readLE(buf.data() + inCoordsOff + X_BYTES + Y_BYTES,  Z_BYTES), Z_BYTES);

            int64_t key = packXYZ(bx, by, bz);
            int outType;
            if      (stumpCoords.count(key))    outType = treeMarkerID;
            else if (foliageCoords.count(key)) outType = foliageMarkerID;
            else                               outType = (int)readLE(buf.data() + inTypeOff, ID_PROP_BYTES);

            int propID = (int)readLE(buf.data() + inPropOff, ID_PROP_BYTES);

            if (customWorld) wLE(outGz, readLE(buf.data(), NS_BYTES), NS_BYTES);
            wLE(outGz, (uint64_t)outType, outIDBytes);
            wLE(outGz, (uint64_t)propID,  outIDBytes);
            wLE(outGz, enc(bx, X_BYTES), X_BYTES);
            wLE(outGz, enc(by, Y_BYTES), Y_BYTES);
            wLE(outGz, enc(bz, Z_BYTES), Z_BYTES);
        }
        gzclose(outGz);
    }

    // ── write blockIds with two new marker entries appended ───────────────
    {
        blockIDs.clear();
        blockIDs.seekg(0);
        std::ofstream dst(outStem + "_trees.blockIds");
        dst << blockIDs.rdbuf();
        dst << treeMarkerID    << "=tree_marker\n";
        dst << foliageMarkerID << "=foliage_marker\n";
    }

    std::cout << "DEBUG stumpCoords: " << stumpCoords.size() << "  foliageCoords: " << foliageCoords.size() << std::endl;
    std::cout << "Tree data written to " << outStem << "_trees.world" << std::endl;

    // close all world files
    gzclose(gz);
    blockIDs.close();
    blockProperties.close();
}

// bypassing need for recursion by utilizing stack data structure to store function calls instead of calling function directly
void createToTree(bool *&visitedMap, const int zenith, block **&thisLayer, int *&IDmap, int *&varietyMap, bool *&isWoodMap, bool *&isYMap, const int i, const int j, const int xSize, const int zSize, std::vector<Tree *> &trees)
{
    std::stack<std::array<int, 2>> blockCalls; // store coords of block to be accessed

    // I AND J ARE INITIAL COORDINATES
    // create tree array
    std::array<int, 2> coords;

    // add first block to stack
    blockCalls.push({i, j});
    visitedMap[i + xSize * j] = true;

    // create tree
    Tree *tree;
    trees.push_back(tree = new Tree(zenith));

    // set tree variety
    tree->variety = getVariety(thisLayer[i + xSize * j]->type);

    while (blockCalls.empty() == false)
    {
        coords = blockCalls.top();
        block *&b = thisLayer[coords[0] + xSize * coords[1]];
        blockCalls.pop();

        tree->head->blocks.push_back(*b); // simply add block to this layer vector

        // add that block to ID map  (negative if its a block that could be part of a stump, positive for leaves)
        int address = coords[0] + xSize * coords[1];

        IDmap[address] = tree->treeID;
        varietyMap[address] = tree->variety;
        isYMap[address] = (isWoodMap[address] = isWoodOrLog(b->type)) && getAxis(b->prop) == Y_;

        // Leaves must not pull in adjacent horizontal logs — that is the mechanism
        // by which building roof beams get swept into a tree during the same-Y BFS.
        bool currentIsLeaf = !isWoodMap[address];

        // emulate calling of function for all surrounding blocks in cross-shaped pattern: (diagonals will not be considered)
        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
            { // MAKE SURE WE DO NOT LOOP OVER THE BLOCK WE PROCESSED ABOVE AGAIN
                if (dx == 0 && dz == 0)
                    continue;
                int ni = coords[0] + dx; // the neighbour indices
                int nj = coords[1] + dz;
                // check bounds
                if (ni >= 0 && ni < xSize && nj >= 0 && nj < zSize)
                {
                    int addressN = ni + xSize * nj;

                    block *bl = thisLayer[addressN];

                    if (bl != nullptr && !visitedMap[addressN] && isAnyTreeBlock(bl->type) && getVariety(bl->type) == tree->variety)
                    {
                        if (currentIsLeaf && isWoodOrLog(bl->type) && getAxis(bl->prop) != Y_)
                            continue; // leaf cannot absorb horizontal logs
                        // push call into stack
                        blockCalls.push({ni, nj});
                        visitedMap[addressN] = true;
                    }
                }
            }
        }
    }
}

void createToTree(bool *&visitedMap, const int zenith, block *&thisLayer, int *&IDmap, const int i, const int j, const int xSize, const int zSize, std::vector<Sub_Tree *> &trees)
{
    std::stack<std::array<int, 2>> blockCalls; // store coords of block to be accessed

    // I AND J ARE INITIAL COORDINATES
    // create tree array
    std::array<int, 2> coords;

    // add first block to stack
    blockCalls.push({i, j});
    visitedMap[i + xSize * j] = true;

    // create tree
    Sub_Tree *tree;
    trees.push_back(tree = new Sub_Tree(zenith));

    // set tree variety
    tree->variety = getVariety(thisLayer[i + xSize * j].type);

    while (blockCalls.empty() == false)
    {
        coords = blockCalls.top();
        block &b = thisLayer[coords[0] + xSize * coords[1]];
        blockCalls.pop();

        tree->head->blocks.push_back(b); // simply add block to this layer vector

        // add that block to ID map  (negative if its a block that could be part of a stump, positive for leaves)
        int address = coords[0] + xSize * coords[1];

        IDmap[address] = tree->sub_treeID;

        // emulate calling of function for all surrounding blocks in cross-shaped pattern: (diagonals will not be considered)
        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
            { // MAKE SURE WE DO NOT LOOP OVER THE BLOCK WE PROCESSED ABOVE AGAIN
                if (dx == 0 && dz == 0)
                    continue;
                int ni = coords[0] + dx; // the neighbour indices
                int nj = coords[1] + dz;
                // check bounds
                if (ni >= 0 && ni < xSize && nj >= 0 && nj < zSize)
                {
                    int addressN = ni + xSize * nj;

                    block &bl = thisLayer[addressN];

                    if (bl.type != -1 && !visitedMap[addressN])
                    {
                        // push call into stack
                        blockCalls.push({ni, nj});
                        visitedMap[addressN] = true;
                    }
                }
            }
        }
    }
}

void addSurroundingToTree(bool *&visitedMap, Tree *tree, Layer *treeLayer, block **&thisLayer, int *&IDmap, int *&varietyMap, bool *&isWoodMap, bool *&isYMap, const int i, const int j, const int xSize, const int zSize)
{
    std::stack<std::array<int, 2>> blockCalls; // store coords of block to be accessed

    // I AND J ARE INITIAL COORDINATES
    // create tree array

    /// start off on this block
    std::array<int, 2> coords = {i, j};

    do
    {
        bool currentIsLeaf = !isWoodMap[coords[0] + xSize * coords[1]];

        // emulate calling of function for all surrounding blocks in cross-shaped pattern: (diagonals will not be considered)
        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
            { // MAKE SURE WE DO NOT LOOP OVER THE BLOCK WE PROCESSED ABOVE AGAIN
                if (dx == 0 && dz == 0)
                    continue;
                int ni = coords[0] + dx; // the neighbour indices
                int nj = coords[1] + dz;
                // check bounds
                if (ni >= 0 && ni < xSize && nj >= 0 && nj < zSize)
                {
                    int addressN = ni + xSize * nj;

                    block *bl = thisLayer[addressN];

                    if (bl != nullptr && !visitedMap[addressN] && isAnyTreeBlock(bl->type) && getVariety(bl->type) == tree->variety)
                    {
                        if (currentIsLeaf && isWoodOrLog(bl->type) && getAxis(bl->prop) != Y_)
                            continue; // leaf cannot absorb horizontal logs
                        // push call into stack
                        blockCalls.push({ni, nj});
                        visitedMap[addressN] = true;
                    }
                }
            }
        }
        if (!blockCalls.empty())
            coords = blockCalls.top();
        else
            break;

        block *&b = thisLayer[coords[0] + xSize * coords[1]];
        blockCalls.pop();

        treeLayer->blocks.push_back(*b); // simply add block to this layer vector

        // add that block to ID map  (negative if its a block that could be part of a stump, positive for leaves)
        int address = coords[0] + xSize * coords[1];

        IDmap[address] = tree->treeID;
        varietyMap[address] = tree->variety;
        isYMap[address] = (isWoodMap[address] = isWoodOrLog(b->type)) && getAxis(b->prop) == Y_;

    } while (true);
}

void addSurroundingToTree(bool *&visitedMap, Sub_Tree *tree, Layer *treeLayer, block *&thisLayer, int *&IDmap, const int i, const int j, const int xSize, const int zSize)
{
    std::stack<std::array<int, 2>> blockCalls; // store coords of block to be accessed

    // I AND J ARE INITIAL COORDINATES
    // create tree array

    /// start off on this block
    std::array<int, 2> coords = {i, j};

    do
    {
        // emulate calling of function for all surrounding blocks in cross-shaped pattern: (diagonals will not be considered)
        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dz = (dx == 0 ? -1 : 0); dz <= (dx == 0 ? 1 : 0); dz++)
            { // MAKE SURE WE DO NOT LOOP OVER THE BLOCK WE PROCESSED ABOVE AGAIN
                if (dx == 0 && dz == 0)
                    continue;
                int ni = coords[0] + dx; // the neighbour indices
                int nj = coords[1] + dz;
                // check bounds
                if (ni >= 0 && ni < xSize && nj >= 0 && nj < zSize)
                {
                    int addressN = ni + xSize * nj;

                    block bl = thisLayer[addressN];

                    if (bl.type != -1 && !visitedMap[addressN])
                    {
                        // push call into stack
                        blockCalls.push({ni, nj});
                        visitedMap[addressN] = true;
                    }
                }
            }
        }
        if (!blockCalls.empty())
            coords = blockCalls.top();
        else
            break;

        block &b = thisLayer[coords[0] + xSize * coords[1]];
        blockCalls.pop();

        treeLayer->blocks.push_back(b); // simply add block to this layer vector

        // add that block to ID map  (negative if its a block that could be part of a stump, positive for leaves)
        int address = coords[0] + xSize * coords[1];

        IDmap[address] = tree->sub_treeID;

    } while (true);
}

bool isSquare(block *const *map, int i, int j, int xSize, int zSize)
{
    int sz = (xSize < zSize) ? xSize : zSize;
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
                            if (map[address] != nullptr)
                            {
                                thisIter = false;
                                break;
                            }
                        }
                        else if (bz == 0 || bz == box - 1)
                        { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation (here we have normal L)
                            if (map[address] != nullptr)
                            {
                                thisIter = false;
                                break;
                            }
                        }
                        else if (map[address] == nullptr)
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

    return false;
}

bool isUprightL(block *const *map, block *const *nextMap, int i, int j, int xSize, int zSize, int &box_x, int &box_z, int &d_x, int &d_z)
{
    for (box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
        for (box_x = 3; box_x <= xSize + 2 - i; box_x++)
            for (d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                for (d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == box_x - 1)
                                    { // depends on L orientation (here we have normal L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == box_x - 1 && bz == 0)
                                    { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation (here we have normal L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_x == 0 && bx == box_x - 1)
                                    { // depends on L orientation (here we have normal L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_z == 0 && bz == 0)
                                    { // depends on L orientation (here we have normal L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bz == 0)
                                    { // depends on L orientation (here we have normal L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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

    return false;
}

bool isBackwardsUprightL(block *const *map, block *const *nextMap, int i, int j, int xSize, int zSize, int &box_x, int &box_z, int &d_x, int &d_z)
{
    for (box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
        for (box_x = 3; box_x <= xSize + 2 - i; box_x++)
            for (d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                for (d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == box_x - 1)
                                    { // depends on L orientation (same as upright L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == 0 && bz == 0)
                                    { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_x == 0 && bx == 0)
                                    { // depends on L orientation
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_z == 0 && bz == 0)
                                    { // depends on L orientation (same as upright L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bz == 0)
                                    { // depends on L orientation (same as upright L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        if (map[address] == nullptr || nextMap[address] != nullptr)
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
    return false;
}

bool isUpsideDownL(block *const *map, block *const *nextMap, int i, int j, int xSize, int zSize, int &box_x, int &box_z, int &d_x, int &d_z)
{
    for (box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
        for (box_x = 3; box_x <= xSize + 2 - i; box_x++)
            for (d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                for (d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == box_x - 1)
                                    { // depends on L orientation (same as backwards L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == 0 && bz == box_z - 1)
                                    { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_x == 0 && bx == 0)
                                    { // depends on L orientation (same as backwards L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_z == 0 && bz == box_z - 1)
                                    { // depends on L orientation
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bz == 0)
                                    { // depends on L orientation (same as backwards L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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

    return false;
}

bool isBackwardsUpsideDownL(block *const *map, block *const *nextMap, int i, int j, int xSize, int zSize, int &box_x, int &box_z, int &d_x, int &d_z)
{

    for (box_z = 3; box_z <= zSize + 2 - j; box_z++) // size of center box (changes over time)
        for (box_x = 3; box_x <= xSize + 2 - i; box_x++)
            for (d_z = 0; d_z <= zSize + 2 - box_z - j; d_z++)     // length of vertical seg (changes over time)
                for (d_x = 0; d_x <= xSize + 2 - box_x - i; d_x++) // length of horizontal seg (changes over time)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == box_x - 1)
                                    { // depends on L orientation (same as upside-down L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bx == box_x - 1 && bz == box_z - 1)
                                    { // (this is the one corner where both d_x and d_z sides stem from) depends on L orientation
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_x == 0 && bx == box_x - 1)
                                    { // depends on L orientation
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (d_z == 0 && bz == box_z - 1)
                                    { // depends on L orientation (same as upside-down L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (bz == 0)
                                    { // depends on L orientation (same as upside-down L)
                                        if (map[address] != nullptr)
                                        {
                                            thisIter = false;
                                            break;
                                        }
                                    }
                                    else if (map[address] == nullptr || nextMap[address] != nullptr)
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
    return false;
}

void push_back_uprightL(std::vector<std::pair<int, int>> &toVerticalDelete, int i, int j, int xSize, int zSize, int xMin, int zMin, int box_x, int box_z, int d_x, int d_z)
{
    (void)xMin; (void)zMin;
    // DRAWING BEGINS HERE --------------------------------

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
                    if (bx != 0 && dz != 0 && bx != box_x - 1)
                    { // depends on L orientation (here we have normal L)
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (bx != 0 && bz != box_z - 1 && (bx != box_x - 1 || bz != 0) && (d_x != 0 || bx != box_x - 1) && (d_z != 0 || bz != 0))
                    { // depends on L orientation (here we have normal L)
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (dx != d_x - 1 && bz != box_z - 1 && bz != 0)
                    { // depends on L orientation (here we have normal L)
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
                }
            }
            else
                break; // go to where bx++
        }
    // DRAWING ENDS HERE -----------------------------------------
}

void push_back_backwardsUprightL(std::vector<std::pair<int, int>> &toVerticalDelete, int i, int j, int xSize, int zSize, int xMin, int zMin, int box_x, int box_z, int d_x, int d_z)
{
    (void)xMin; (void)zMin;
    // DRAWING BEGINS HERE --------------------------------
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
                    if (bx != 0 && dz != 0 && bx != box_x - 1)
                    { // depends on L orientation (same as upright L)
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (bx != box_x - 1 && bz != box_z - 1 && (bx != 0 || bz != 0) && (d_x != 0 || bx != 0) && (d_z != 0 || bz != 0))
                    { // depends on L orientation
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (dx != 0 && bz != box_z - 1 && bz != 0)
                    { // depends on L orientation
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
                }
            }
            else
                break; // go to where bx++
        }

    // DRAWING ENDS HERE -----------------------------------------
}

void push_back_upsideDownL(std::vector<std::pair<int, int>> &toVerticalDelete, int i, int j, int xSize, int zSize, int xMin, int zMin, int box_x, int box_z, int d_x, int d_z)
{
    (void)xMin; (void)zMin;
    // DRAWING BEGINS HERE --------------------------------

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
                    if (bx != 0 && dz != d_z - 1 && bx != box_x - 1)
                    { // depends on L orientation
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (bx != box_x - 1 && bz != 0 && (bx != 0 || bz != box_z - 1) && (d_x != 0 || bx != 0) && (d_z != 0 || bz != box_z - 1))
                    { // depends on L orientation
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (dx != 0 && bz != box_z - 1 && bz != 0)
                    { // depends on L orientation (same as backwards L)
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
                }
            }
            else
                break; // go to where bx++
        }

    // DRAWING ENDS HERE -----------------------------------------
}

void push_back_backwardsUpsideDownL(std::vector<std::pair<int, int>> &toVerticalDelete, int i, int j, int xSize, int zSize, int xMin, int zMin, int box_x, int box_z, int d_x, int d_z)
{
    (void)xMin; (void)zMin;
    // DRAWING BEGINS HERE --------------------------------

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
                    if (bx != 0 && dz != d_z - 1 && bx != box_x - 1)
                    { // depends on L orientation (same as upside-down L)
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (bx != 0 && bz != 0 && (bx != box_x - 1 || bz != box_z - 1) && (d_x != 0 || bx != box_x - 1) && (d_z != 0 || bz != box_z - 1))
                    { // depends on L orientation
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
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
                    if (dx != d_x - 1 && bz != box_z - 1 && bz != 0)
                    { // depends on L orientation
                        toVerticalDelete.push_back({addressX, addressZ});
                    }
                }
            }
            else
                break; // go to where bx++
        }

    // DRAWING ENDS HERE -----------------------------------------
}
