/*##########################################################################
#
# Copyright (c) 2015-2016 European Synchrotron Radiation Facility
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# ###########################################################################*/
#ifndef __mc_HPP__
#define __mc_HPP__

#include <iostream>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>


extern const int MCTriangleTable[256][16];
extern const unsigned int MCEdgeIndexToCoordOffsets[12][4];

#define DEPTH_IDX 0
#define HEIGHT_IDX 1
#define WIDTH_IDX 2

/** Class Marching cubes
 *
 * Implements the marching cube algorithm and provides an API to process
 * data image by image.
 */
template <typename FloatType>
class MarchingCubes {
public:
    /** Create a marching cube object.
     *
     * @param level Level at which to build the isosurface
     */
    MarchingCubes(FloatType level);

    ~MarchingCubes();

    /** Process a 3D scalar field
     *
     * @param data Pointer to the data set
     * @param depth The 1st of the data set
     * @param height The 2nd dimension of the data set
     * @param width The 3rd dimension of the data set
     *              (tightly packed in memory)
     */
    void process(FloatType * data,
                 unsigned int depth,
                 unsigned int height,
                 unsigned int width);

    /** Init dimension of slices
     *
     * @param height Height in pixels of the slices
     * @param width Width in pixels of the slices
     */
    void set_slice_size(unsigned int height,
                        unsigned int width);

    /** Process a slice (i.e., an image)
     *
     * The size of the images MUST match height and width provided to
     * set_slice_size.
     *
     * The marching cube process 2 consecutive images at a time.
     * A slice provided as next parameter MUST be provided as current
     * parameter for the next call.
     * Example with 3 images:
     *
     * float * img1;
     * float * img2;
     * float * img3;
     * ...
     * mc = MarchingCubes<float>(100.);
     * mc.set_slice_size(10, 10);
     * mc.process_slice(img1, img2);
     * mc.process_slice(img2, img3);
     * mc.finish_process();
     *
     * @param slice0 Pointer to the nth slice data
     * @param slice1 Pointer to the (n+1)th slice of data
     */
    void process_slice(FloatType * slice0,
                       FloatType * slice1);

    /** Clear marching cube processing internal cache. */
    void finish_process();

    /** Reset all internal data and counters. */
    void reset();

    /** Vertices of the isosurface (x, y, z) */
    std::vector<FloatType> vertices;

    /** Approximation of normals at the vertices (nx, ny, nz)
     *
     * Current implementation provides coarse (but fast) normals computation
     */
    std::vector<FloatType> normals;

    /** Triangle indices */
    std::vector<unsigned int> indices;

    unsigned int depth; /**< Number of images currently processed */
    unsigned int height; /**< Images height in pixels */
    unsigned int width; /**< Images width in pixels */

    /** Sampling of the data (depth, height, width)
     *
     * Default: 1, 1, 1
     */
    unsigned int sampling[3];

    FloatType isolevel; /**< Iso level to use */
    bool invert_normals; /**< True to inverse gradient as normals */

private:

    /** Start to build isosurface starting with first slice
     *
     * Bootstrap cache edge_indices
     *
     * @param slice The first slice of the data
     * @param next The second slice
     */
    void first_slice(FloatType * slice,
                     FloatType * next);

    /** Process an edge
     *
     * @param value0 Data at 'begining' of edge
     * @param value Data at 'end' of edge
     * @param depth Depth coordinate of the edge position
     * @param row Row coordinate of the edge
     * @param col Column coordinate of the edge
     * @param direction Direction of the edge: 0 for x, 1 for y and 2 for z
     * @param previous
     * @param current
     * @param next
     */
    void process_edge(FloatType value0,
                      FloatType value,
                      unsigned int depth,
                      unsigned int row,
                      unsigned int col,
                      unsigned int direction,
                      FloatType * previous,
                      FloatType * current,
                      FloatType * next);

    /** Return the bit mask of cube corners <= the iso-value.
     *
     * @param slice1 1st slice of the cube to consider
     * @param slice2 2nd slice of the cube to consider
     * @param row Row of the cube to consider
     * @param col Column of the cube to consider
     * @return The bit mask of cube corners <= the iso-value
     */
    unsigned char get_cell_code(FloatType * slice1,
                                FloatType * slice2,
                                unsigned int row,
                                unsigned int col);

    /** Compute an edge index from position and edge direction.
     *
     * @param depth Depth of the origin of the edge
     * @param row Row of the origin of the edge
     * @param col Column of the origin of the edge
     * @param direction 0 for x, 1 for y, 2 for z
     * @return The (4D) index of the edge
     */
    unsigned int edge_index(unsigned int depth,
                            unsigned int row,
                            unsigned int col,
                            unsigned int direction);

    /** For each dimension, a map from edge index to vertex index
     *
     * This caches indices for previously processed slice.
     *
     * Edge index is the linearized position of the edge using size + 1
     * in all dimensions as coordinates plus the direction as 4th coord.
     * WARNING: direction 0 for x, 1 for y and 2 for z
     */
    std::map<unsigned int, unsigned int> * edge_indices;
};


/* Implementation */

template <typename FloatType>
MarchingCubes<FloatType>::MarchingCubes(FloatType level)
{
    this->edge_indices = 0;
    this->reset();
    this->height = 0;
    this->width = 0;
    this->isolevel = level;
    this->invert_normals = true;
    this->sampling[0] = 1;
    this->sampling[1] = 1;
    this->sampling[2] = 1;
}

template <typename FloatType>
MarchingCubes<FloatType>::~MarchingCubes()
{
}

template <typename FloatType>
void
MarchingCubes<FloatType>::reset()
{
    this->depth = 0;
    this->vertices.clear();
    this->normals.clear();
    this->indices.clear();
    if (this->edge_indices != 0) {
        delete this->edge_indices;
        this->edge_indices = 0;
    }
}

template <typename FloatType>
void
MarchingCubes<FloatType>::finish_process()
{
    if (this->edge_indices != 0) {
        delete this->edge_indices;
        this->edge_indices = 0;
    }
}


template <typename FloatType>
void
MarchingCubes<FloatType>::process(FloatType * data,
                                  unsigned int depth,
                                  unsigned int height,
                                  unsigned int width)
{
    unsigned int size = height * width * this->sampling[DEPTH_IDX];

    /* number of slices minus - 1 to process */
    const unsigned int nb_slices = (depth - 1) / this->sampling[DEPTH_IDX];

    this->reset();
    this->set_slice_size(height, width);

    for (unsigned int index=0; index < nb_slices; index++) {
        FloatType * slice0 = data + (index * size);
        FloatType * slice1 = slice0 + size;

        this->process_slice(slice0, slice1);
    }
    this->finish_process();

    this->depth = depth;
}


template <typename FloatType>
void
MarchingCubes<FloatType>::set_slice_size(unsigned int height,
                                         unsigned int width)
{
    this->reset();
    this->height = height;
    this->width = width;
}


template <typename FloatType>
void
MarchingCubes<FloatType>::process_slice(FloatType * slice0,
                                        FloatType * slice1)
{
    unsigned int row, col;

    if (this->edge_indices == 0) {
        /* No previously processed slice, bootstrap */
        this->first_slice(slice0, slice1);
    }

    /* Keep reference to cache from previous slice */
    std::map<unsigned int, unsigned int> * previous_edge_indices =
        this->edge_indices;

    /* Init cache for this slice */
    this->edge_indices = new std::map<unsigned int, unsigned int>();

    /* Loop over slice to add vertices */
    for (row=0; row < this->height; row += this->sampling[HEIGHT_IDX]) {
        unsigned int line_index = row * this->width;

        for (col=0; col < this->width; col += this->sampling[WIDTH_IDX]) {
            unsigned int item_index = line_index + col;

            FloatType value0 = slice1[item_index];

            /* Test forward edges and add vertices in the current slice plane */
            if (col < (width - this->sampling[WIDTH_IDX])) {
                FloatType value = slice1[item_index + this->sampling[WIDTH_IDX]];

                this->process_edge(value0, value, this->depth, row, col, 0,
                                   slice0, slice1, 0);
            }

            if (row < (height - this->sampling[HEIGHT_IDX])) {
                /* Value from next line*/
                FloatType value = slice1[item_index + this->width * this->sampling[HEIGHT_IDX]];

                this->process_edge(value0, value, this->depth, row, col, 1,
                                   slice0, slice1, 0);
            }

            /* Test backward edges and add vertices in z direction */
            {
                FloatType value = slice0[item_index];

                /* Expect forward edge, so pass: previous, current */
                this->process_edge(value, value0,
                                   this->depth - this->sampling[DEPTH_IDX],
                                   row, col, 2,
                                   0, slice0, slice1);
            }
            
        }
    }

    /* Loop over cubes to add triangle indices */
    for (row=0; row < this->height - this->sampling[HEIGHT_IDX]; row += this->sampling[HEIGHT_IDX]) {
        for (col=0; col < this->width - this->sampling[WIDTH_IDX]; col += this->sampling[WIDTH_IDX]) {
            unsigned char code = this->get_cell_code(slice0, slice1,
                                                     row, col);

            if (code == 0) {
                continue;
            }

            const int * edgeIndexPtr = &MCTriangleTable[code][0];
            for (; *edgeIndexPtr >= 0; edgeIndexPtr++) {
                const unsigned int * offsets = \
                    MCEdgeIndexToCoordOffsets[*edgeIndexPtr];

                unsigned int edge_index = this->edge_index(
                        this->depth - this->sampling[DEPTH_IDX] + offsets[DEPTH_IDX] * this->sampling[DEPTH_IDX],
                        row + offsets[HEIGHT_IDX] * this->sampling[HEIGHT_IDX],
                        col + offsets[WIDTH_IDX] * this->sampling[WIDTH_IDX],
                        offsets[3]);

                /* Add vertex index to the list of indices */
                std::map<unsigned int, unsigned int>::iterator it, end;
                if (offsets[DEPTH_IDX] == 0 && offsets[3] != 2) {
                    it = previous_edge_indices->find(edge_index);
                    end = previous_edge_indices->end();
                } else {
                    it = this->edge_indices->find(edge_index);
                    end = this->edge_indices->end();
                }
                if (it == end) {
                    throw std::runtime_error(
                        "Internal error: cannot build triangle indices.");
                }
                else {
                    this->indices.push_back(it->second);
                }
            }

        }
    }

    /* Clean-up previous slice cache */
    delete previous_edge_indices;

    this->depth += this->sampling[DEPTH_IDX];
}


template <typename FloatType>
void
MarchingCubes<FloatType>::first_slice(FloatType * slice,
                                      FloatType * next)
{
    /* Init cache for this slice */
    this->edge_indices = new std::map<unsigned int, unsigned int>();

    unsigned int row, col;

    /* Loop over slice, and add isosurface vertices in the slice plane */
    for (row=0; row < this->height; row += this->sampling[HEIGHT_IDX]) {
        unsigned int line_index = row * this->width;

        for (col=0; col < this->width; col += this->sampling[WIDTH_IDX]) {
            unsigned int item_index = line_index + col;

            /* For each point test forward edges */
            FloatType value0 = slice[item_index];

            if (col < (width - this->sampling[WIDTH_IDX])) {
                FloatType value = slice[item_index + this->sampling[WIDTH_IDX]];

                this->process_edge(value0, value, this->depth, row, col, 0,
                                   0, slice, next);
            }

            if (row < (height - this->sampling[HEIGHT_IDX])) {
                /* Value from next line */
                FloatType value = slice[item_index + this->width * this->sampling[HEIGHT_IDX]];

                this->process_edge(value0, value, this->depth, row, col, 1,
                                   0, slice, next);
            }
        }
    }

    this->depth += this->sampling[DEPTH_IDX];
}


template <typename FloatType>
inline unsigned int
MarchingCubes<FloatType>::edge_index(unsigned int depth,
                                     unsigned int row,
                                     unsigned int col,
                                     unsigned int direction)
{
    return ((depth * (this->height + 1) + row) *
            (this->width + 1) + col) * 3 + direction;
}


template <typename FloatType>
inline void
MarchingCubes<FloatType>::process_edge(FloatType value0,
                                       FloatType value,
                                       unsigned int depth,
                                       unsigned int row,
                                       unsigned int col,
                                       unsigned int direction,
                                       FloatType * previous,
                                       FloatType * current,
                                       FloatType * next)
{
    if ((value0 <= this->isolevel) ^ (value <= this->isolevel)) {

        /* Crossing iso-surface, store it */
        FloatType offset = \
            (this->isolevel - value0) / (value - value0);

        /* Store edge to vertex index correspondance */
        unsigned int edge_index = this->edge_index(depth, row, col, direction);
        (*this->edge_indices)[edge_index] = this->vertices.size() / 3;

        /* Store vertex as (x, y, z) */
        if (direction == 0) {
            this->vertices.push_back(
                (FloatType) col + offset * this->sampling[WIDTH_IDX]);
            this->vertices.push_back((FloatType) row);
            this->vertices.push_back((FloatType) depth);
        }
        else if (direction == 1) {
            this->vertices.push_back((FloatType) col);
            this->vertices.push_back(
                (FloatType) row + offset * this->sampling[HEIGHT_IDX]);
            this->vertices.push_back((FloatType) depth);
        }
        else if (direction == 2) {
            this->vertices.push_back((FloatType) col);
            this->vertices.push_back((FloatType) row);
            this->vertices.push_back(
                (FloatType) depth + offset * this->sampling[DEPTH_IDX]);
        } else {
            throw std::runtime_error(
                "Internal error: dimension > 3, never event.");
        }

        /* Store normal as (nx, ny, nz) */
        FloatType nx, ny, nz;
        FloatType * slice0;
        FloatType * slice1;

        if (previous != 0) {
            slice0 = previous;
            slice1 = current;
        } else { /* next != 0 */
            slice0 = current;
            slice1 = next;
        }

        unsigned int row_offset = this->width * this->sampling[HEIGHT_IDX];

        if (direction == 0) {
            nx = value - value0;

            { /* ny */
                unsigned int item, item_next_col;

                item = row * this->width + col;
                if (row >= this->height - this->sampling[HEIGHT_IDX]) {
                    /* For last row, use previous row */
                    item -= row_offset;
                }
                if (col >= this->width - this->sampling[WIDTH_IDX]) {
                    /* For last column, use previous column */
                    item -= this->sampling[WIDTH_IDX];
                }
                item_next_col = item + this->sampling[WIDTH_IDX];

                ny = ((1. - offset) * (current[item + row_offset] -
                                       current[item]) +
                     offset * (current[item_next_col + row_offset] -
                               current[item_next_col]));
            }

            { /* nz */
                unsigned int item, item_next_col;

                item = row * this->width + col;
                if (col >= this->width - this->sampling[WIDTH_IDX]) {
                    /* For last column, use previous column */
                    item -= this->sampling[WIDTH_IDX];
                }
                item_next_col = item + this->sampling[WIDTH_IDX];

               nz = ((1. - offset) * (slice1[item] - slice0[item]) +
                     offset * (slice1[item_next_col] - slice0[item_next_col]));
            }

        } else if (direction == 1) {
            { /* nx */
                unsigned int item, item_next_row;

                item = row * this->width + col;
                if (row >= this->height - this->sampling[HEIGHT_IDX]) {
                    /* For last row, use previous row */
                    item -= row_offset;
                }
                if (col >= this->width - this->sampling[WIDTH_IDX]) {
                    /* For last column, use previous column */
                    item -= this->sampling[WIDTH_IDX];
                }

                item_next_row = item + row_offset;

                nx = ((1. - offset) * (current[item + this->sampling[WIDTH_IDX]] - current[item]) +
                      offset * (current[item_next_row + this->sampling[WIDTH_IDX]] - current[item_next_row]));
            }

            ny = value - value0;

            { /* nz */
                unsigned int item, item_next_row;
                
                item = row * this->width + col;
                if (row >= this->height - this->sampling[HEIGHT_IDX]) {
                    /* For last row, use previous row */
                    item -= row_offset;
                }
                item_next_row = item + row_offset;

               nz = ((1. - offset) * (slice1[item] - slice0[item]) +
                     offset * (slice1[item_next_row] - slice0[item_next_row]));
            }

        } else { /* direction == 2 */
            /* Previous should always be 0, only here in case this changes */
            FloatType * other_slice = (previous != 0) ? previous : next;

            { /* nx */
                unsigned int item, item_next_col;

                item = row * this->width + col;
                if (col >= this->width - this->sampling[WIDTH_IDX]) {
                    /* For last column, use previous column */
                    item -= this->sampling[WIDTH_IDX];
                }
                item_next_col = item + this->sampling[WIDTH_IDX];

                nx = ((1. - offset) * (current[item_next_col] - current[item]) +
                      offset * (other_slice[item_next_col] - other_slice[item]));
            }

            { /* ny */
                unsigned int item, item_next_row;

                item = row * this->width + col;
                if (row >= this->height - this->sampling[HEIGHT_IDX]) {
                    /* For last row, use previous row */
                    item -= row_offset;
                }
                item_next_row = item + row_offset;

                ny = ((1. - offset) * (current[item_next_row] - current[item]) +
                      offset * (other_slice[item_next_row] - other_slice[item]));
            }

            nz = value - value0;
        }

        /* apply sampling scaling */
        nx /= (FloatType) this->sampling[2];
        ny /= (FloatType) this->sampling[1];
        nz /= (FloatType) this->sampling[0];

        /* normalisation */
        FloatType norm = sqrt(nx * nx + ny * ny + nz * nz);
        if (this->invert_normals) { /* Normal inversion */
            norm *= -1.;
        }

        if (norm != 0) {
            nx /= norm;
            ny /= norm;
            nz /= norm;
        }
        this->normals.push_back(nx);
        this->normals.push_back(ny);
        this->normals.push_back(nz);
    }
}


template <typename FloatType>
inline unsigned char
MarchingCubes<FloatType>::get_cell_code(FloatType * slice1,
                                        FloatType * slice2,
                                        unsigned int row,
                                        unsigned int col)
{
    unsigned int item = row * this->width + col;
    unsigned int item_next_row = item + this->width * this->sampling[HEIGHT_IDX];
    unsigned char code = 0;

    /* First slice */
    if (slice1[item] <= this->isolevel) {
        code |= 1 << 0;
    }
    if (slice1[item + this->sampling[WIDTH_IDX]] <= this->isolevel) {
        code |= 1 << 1;
    }
    if (slice1[item_next_row + this->sampling[WIDTH_IDX]] <= this->isolevel) {
        code |= 1 << 2;
    }
    if (slice1[item_next_row] <= this->isolevel) {
        code |= 1 << 3;
    }

    /* Second slice */
    if (slice2[item] <= this->isolevel) {
        code |= 1 << 4;
    }
    if (slice2[item + this->sampling[WIDTH_IDX]] <= this->isolevel) {
        code |= 1 << 5;
    }
    if (slice2[item_next_row + this->sampling[WIDTH_IDX]] <= this->isolevel) {
        code |= 1 << 6;
    }
    if (slice2[item_next_row] <= this->isolevel) {
        code |= 1 << 7;
    }

    return code;
}

#endif /*__mc_HPP__*/
