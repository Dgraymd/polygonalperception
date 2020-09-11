#include "Grid.h"
#include "globals.h"
#include "util/Statistics.h"
#include "util/ColorUtil.h"
#include "util/Vec3.h"
#include <QFile>

// This is a generic DIM dimensional uniform grid construction class.
// Grid nodes are evenly distributed along each axis of the bounded
// input space such that the first node is located at the minimum and
// the last node is located at the maximum. To use the Grid, first
// provide the grid parameters using setDim(), setN(), setMin(), and
// setMax(). Then, call rasterize()! It is crucial that you call
// rasterize() after all parameters have been provided and before you
// make use of the Grid in any way. Example:
//
// Grid grid; // Construct a grid object.
// grid.setDim(3); // Set the number of dimensions.
// uint N[3] = {101,201,301};
// grid.setN(N); // Set the numer of nodes per dimension.
// double min[3] = {config.xMin, config.yMin, config.zMin};
// grid.setMin(min); // Set the minimum values per dimension.
// double max[3] = {config.xMax, config.yMax, config.zMax};
// grid.setMax(max); // Set the maximum values per dimension.
// grid.rasterize(); // Compute the grid representation.
//
// Now you can access the grid node coordinates using either a DIM
// dimensional unsigned integer index (dim index), or a one dimensional
// unsigned integer index (flat index) between 0 and nodeCount that
// enummerates all grid nodes. Example:
//
// for (uint n=0; n < grid.getNodeCount(); n++)
// {
//    QVector<double> coordinates = grid.getNodeCoordinates(n);
//    QVector<uint> idx = grid.convertIndex(n);
//    QVector<double> sameCoordinates = grid.getNodeCoordinates(idx);
// }
//
// As a general rule, points in the grid space are represented as
// QVector<double>, dim indexes are represented as QVector<uint>, and
// flat indexes are represented as uint.
//
// A number of other nifty methods allow you to convert between the dim
// and the flat index representations, query the nearest node index of
// any point on the grid, retrieve all eveloping nodes of a query point,
// retrieve the neighborhood of a grid node, and to save and load the
// grid to and from a binary file.
//
// When setting up the grid, heap memory needs to be allocated (setDim()
// and rasterize()) and this is not a real time capable operation. Grid
// construction should happen outside of time critical loops.

Grid::Grid()
{
    nodeCount = 0;
}

// Sets the number of dimensions of the grid.
void Grid::setDim(uint d)
{
    DIM = d;
    N.resize(DIM);
    max.resize(DIM);
    min.resize(DIM);
    raster.resize(DIM);
    cumN.resize(DIM);
    stride.resize(DIM);
    strideinv.resize(DIM);
    _idx.resize(DIM);
    _x.resize(DIM);
}

// Returns DIM, the number of dimensions.
uint Grid::getDim() const
{
    return DIM;
}

// Sets the min boundaries of the DIM dimensional data range.
// The argument double will be set as minimum for all dimensions.
void Grid::setMin(double minn)
{
    //min.fill(minn);
}

// Sets the max boundaries of the DIM dimensional data range.
// The argument double will be set as maximum for all dimensions.
void Grid::setMax(double maxx)
{
    //max.fill(maxx);
}

// Sets the min boundaries of the DIM dimensional data range.
// It is your responsibility to make sure the pointer passed as the argument
// points to a valid vector of parameters of size DIM.
void Grid::setMin(const double *minn)
{
    for (uint d = 0; d < DIM; d++)
        min[d] = minn[d];
}

// Sets the max boundaries of the DIM dimensional data range.
// It is your responsibility to make sure the pointer passed as the argument
// points to a valid vector of parameters of size DIM.
void Grid::setMax(const double *maxx)
{
    for (uint d = 0; d < DIM; d++)
        max[d] = maxx[d];
}

// Returns the min data range boundaries.
const double* Grid::getMin() const
{
    return min.data();
}

// Returns the max data range boundaries.
const double* Grid::getMax() const
{
    return max.data();
}

// Sets N, the number of nodes per dimension. This method sets N to be the same
// for every dimension and thus creates a uniform grid.
void Grid::setN(uint N_)
{
    uint N__[DIM];
    for (uint d=0; d < DIM; d++)
        N__[d] = N_;
    setN(N__);
}

// Sets N, the number of nodes per dimension. This method sets a different N for
// each individual dimension.
void Grid::setN(const uint* N_)
{
    for (uint d=0; d < DIM; d++)
        N[d] = N_[d];
}

// Returns the number of nodes per dimension. The size of the vector is DIM, of course.
const uint* Grid::getN() const
{
    return N.data();
}

// Calculates the raster of the grid coordinates. The grid nodes are distributed between the
// respective min and max values of each dimension such that the first node is located at the
// min and the last node is located at the max. Dim, N, min, and max must be set before
// computing the raster. Make sure to set the grid parameters first and then call this method
// to prepare the grid before using it.
void Grid::rasterize()
{
    nodeCount = N[0];
    for (uint d = 1; d < DIM; d++)
        nodeCount *= N[d];

    // Accumulate the number of nodes per dimension to compute a "stride" in index space.
    // This speeds up index coversions.
    cumN[0] = 1;
    for (uint d = 1; d < DIM; d++)
        cumN[d] = cumN[d-1]*N[d-1];

    // Compute the stride and the raster in the grid space.
    for (uint d = 0; d < DIM; d++)
    {
        double l = max[d]-min[d];
        stride[d] = l/(N[d]-1);
        strideinv[d] = 1.0/stride[d];
        raster[d].resize(N[d]);
        for (uint n = 0; n < N[d]; n++)
            raster[d][n] = min[d]+n*stride[d];
    }
}

// Returns a pointer to a DIM sized vector that contains the strides for each dimension.
// The stride is the size of a cell in the respective dimension.
const double* Grid::getStride() const
{
    return stride.data();
}

// Returns the total number of Grid nodes.
uint Grid::getNodeCount() const
{
    return nodeCount;
}

// Converts a DIM dimensional index to a flat index.
uint Grid::convertIndex(const uint *idx) const
{
    uint k = idx[0];
    for (uint d = 1; d < DIM; d++)
    {
        k += idx[d]*cumN[d];
    }
    return k;
}

// Converts a flat index to a DIM dimensional index.
// Invalidates all dim index references returned so far.
const uint* Grid::convertIndex(uint idx) const
{
    uint v = idx;
    for (uint d = 0; d < DIM; d++)
    {
        _idx[d] = v % N[d];
        v = (uint)(v/N[d]);
    }
    return _idx.data();
}

// Computes the "bottom left" DIM dimensional index of the grid node that contains point x.
// Invalidates all dim index references returned so far.
const uint* Grid::getNodeIndexBl(const double* x) const
{
    for (uint d = 0; d < DIM; d++)
        _idx[d] = (uint)qBound(0, (int)((x[d]-min[d])*strideinv[d]), (int)N[d]-2);
    return _idx.data();
}

// Computes the DIM dimensional index of the grid node closest to the point x.
// Invalidates all dim index references returned so far.
const uint* Grid::getNodeIndex(const double* x) const
{
    for (uint d = 0; d < DIM; d++)
        _idx[d] = (uint)qBound(0, qRound((x[d]-min[d])*strideinv[d]), (int)N[d]-1);
    return _idx.data();
}

// Computes the "bottom left" flat index of the grid node that contains point x.
// Invalidates all dim index references returned so far.
uint Grid::getNodeFlatIndexBl(const double* x) const
{
    for (uint d = 0; d < DIM; d++)
        _idx[d] = (uint)qBound(0, (int)((x[d]-min[d])*strideinv[d]), (int)N[d]-2);
    return convertIndex(_idx.data());
}

// Computes the flat index of the grid node closest to the point x.
// Invalidates all dim index references returned so far.
uint Grid::getNodeFlatIndex(const double* x) const
{
    for (uint d = 0; d < DIM; d++)
        _idx[d] = (uint)qBound(0, qRound((x[d]-min[d])*strideinv[d]), (int)N[d]-1);
    return convertIndex(_idx.data());
}

// Returns the Grid coordinates of the node specified by the DIM dimensional index.
// Invalidates all point references returned so far.
const double* Grid::getNodeCoordinates(const uint* idx) const
{
    for (uint d = 0; d < DIM; d++)
        _x[d] = raster[d][idx[d]];
    return _x.data();
}

// Returns the Grid coordinates of the node specified by the flat index.
// Invalidates all point references returned so far.
const double* Grid::getNodeCoordinates(uint n) const
{
    const uint* idx = convertIndex(n);
    for (uint d = 0; d < DIM; d++)
        _x[d] = raster[d][idx[d]];
    return _x.data();
}

// Enumerates the flat indexes of the nodes in a neighborhood of radius r around
// the node specified by the flat index n. The radius is interpreted as the Manhattan
// distance in index space where directly neighboring nodes have the distance 1.
// When radius = 0, the method returns only the node n itself.
// This method has to allocate heap memory for the returned vector of flat indices
// and is thus not real time capable. Invalidates all index references returned so far.
QVector<uint> Grid::enumerateNeighborHood(uint n, uint radius) const
{
    return enumerateNeighborHood(convertIndex(n), radius);
}

// Enumerates the flat indexes of the nodes in a neighborhood of radius r around
// the node specified by DIM index idx. The radius is interpreted as the Manhattan
// distance in index space where directly neighboring nodes have the distance 1.
// When radius = 0, the method returns only the node n itself.
// This method has to allocate heap memory for the returned vector of flat indices
// and is thus not real time capable. Invalidates all index references returned so far.
QVector<uint> Grid::enumerateNeighborHood(const uint *idx, uint radius) const
{
    uint center = convertIndex(idx);

    // Using the radius, determine the min and max boundaries of the enveloping
    // hypercube in index space while respecting the grid boundaries.
    uint bmin[DIM];
    uint bmax[DIM];
    for (uint d = 0; d < DIM; d++)
    {
        bmin[d] = (idx[d] < radius)? (uint)0 : (idx[d] - radius);
        //bmin[d] = qMax(idx[d]-radius, (uint)0);
        bmax[d] = qMin(idx[d]+radius,  N[d]-1);
        //qDebug() << idx[d] << radius << bmin[d] << bmax[d];
    }

    // Count the neighbors.
    uint neighbors = 1;
    for (uint d = 0; d < DIM; d++)
        neighbors *= bmax[d]-bmin[d]+1;
    // Generate the flat coordinates.
    QVector<uint> neighborhood;
    for (uint d = 0; d < DIM; d++)
        _idx[d] = bmin[d];

    for (uint i = 0; i < neighbors; i++)
    {
        uint c = convertIndex(_idx.data());
        if (c != center)
            neighborhood << c;
        uint d = 0;
        while (d < DIM)
        {
            _idx[d]++;
            if (_idx[d] <= bmax[d])
                break;
            _idx[d] = bmin[d];
            d++;
        }
    }

    return neighborhood;
}

// Returns the flat node indixes of the hypercube that contains the given point x.
// If the radius > 0, it expands the enveloping hypercube by radius in index space
// and returns all included node indexes.
// This method has to allocate heap memory for the returned vector of flat indices
// and is thus not real time capable. Invalidates all index references returned so far.
QVector<uint> Grid::getEnvelopingNodes(const double* x, uint radius) const
{
    // Determine the bl node of the hypercube that contains x.
    const uint* idx = getNodeIndexBl(x);

    // Using the radius, determine the min and max boundaries of the enveloping
    // hypercube in index space while respecting the grid boundaries.
    uint bmin[DIM];
    uint bmax[DIM];
    for (uint d = 0; d < DIM; d++)
    {
        bmin[d] = qMax(idx[d]-radius, (uint)0);
        bmax[d] = qMin(idx[d]+radius+1, N[d]-1);
    }

    // Count the nodes.
    uint neighbors = 1;
    for (uint d = 0; d < DIM; d++)
        neighbors *= bmax[d]-bmin[d]+1;

    // Generate the flat coordinates.
    QVector<uint> neighborhood;
    for (uint d = 0; d < DIM; d++)
        _idx[d] = bmin[d];
    for (uint i = 0; i < neighbors; i++)
    {
        neighborhood << convertIndex(_idx.data());

        uint d = 0;
        while (d < DIM)
        {
            _idx[d]++;
            if (_idx[d] <= bmax[d])
                break;
            _idx[d] = bmin[d];
            d++;
        }
    }

    return neighborhood;
}

// Returns true if the given cartesian point is within the boundaries of the grid.
bool Grid::containsPoint(const double *x) const
{
    for (uint d = 0; d < DIM; d++)
        if (x[d] > max[d] || x[d] < min[d])
            return false;

    return true;
}

// Returns a uniformly sampled point from the grid space.
// Invalidates all point references returned so far.
const double *Grid::uniformSample() const
{
    for (uint d = 0; d < DIM; d++)
        _x[d] = Statistics::uniformSample(min[d], max[d]);
    return _x.data();
}

// Loads a binary saved Grid.
void Grid::load(QString name)
{
    QString fileName = name.section(".", 0, 0);
    if (fileName.isEmpty())
    {
        qDebug() << "Grid::load(): invalid file name" << name;
        return;
    }
    fileName += ".gri";

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Grid::load(): Could not open file" << file.fileName();
        return;
    }

//    QDataStream in(&file);
//    in >> DIM;
//    in >> min;
//    in >> max;
//    in << N;
//    file.close();
//    rasterize();
}

// Saves the Grid in a binary file.
void Grid::save(QString name) const
{
    QString fileName = name.section(".", 0, 0);
    if (fileName.isEmpty())
    {
        qDebug() << "Grid::save(): invalid file name" << name;
        return;
    }
    fileName += ".gri";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "Grid::save(): could not write to file" << file.fileName();
        return;
    }

//    QDataStream out(&file);
//    out << DIM;
//    out << min;
//    out << max;
//    out << N;
//    file.close();
}

// Prints a textual represenation of the Grid node coordinates to the console.
void Grid::printGridNodes() const
{
    qDebug() << "d n coord";
    for (uint d = 0; d < DIM; d++)
    {
        qDebug() << "-----------";
        for (uint n = 0; n < N[d]; n++)
            qDebug() << d << n << raster[d][n];
    }
}

// QPainter code that draws the grid node coordinates in a 2D space.
void Grid::drawGridNodes(QPainter* painter, uint sampleFactor) const
{
    if (DIM < 2)
        return;

    sampleFactor = qMax(sampleFactor, (uint)1);

    double ws = 0.05;

    painter->save();
    painter->setPen(colorUtil.pen);
    painter->setBrush(colorUtil.brush);
    for (uint j = 0; j < N[1]; j=j+sampleFactor)
        for (uint i = 0; i < N[0]; i=i+sampleFactor)
            painter->drawEllipse(QPointF(raster[0][i], raster[1][j]), ws, ws);
    painter->restore();
}