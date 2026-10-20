/**
 * @file NeuralMatrix.h
 * @brief Matrix operations used by native neural networks.
 * @ingroup Neural
 */
#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace NoJob::Neural {

/** @brief Row-major matrix storing single-precision neural network values. */
class Matrix {
public:
    Matrix() = default;

    /** @throws std::invalid_argument If either dimension is zero. */
    Matrix(size_t rows, size_t cols, float initial = 0.0f)
        : rows_(rows), cols_(cols), data_(rows * cols, initial) {
        if (rows == 0 || cols == 0)
            throw std::invalid_argument("Empty matrix dimension");
    }

    size_t Rows() const { return rows_; }
    size_t Cols() const { return cols_; }

    float& operator()(size_t row, size_t col) { return data_.at(row * cols_ + col); }
    float operator()(size_t row, size_t col) const { return data_.at(row * cols_ + col); }

    const std::vector<float>& Data() const { return data_; }
    std::vector<float>& Data() { return data_; }

    /** @brief Multiplies two compatible matrices. */
    static Matrix Multiply(const Matrix& a, const Matrix& b) {
        if (a.cols_ != b.rows_)
            throw std::invalid_argument("Matrix multiply dimensions");

        Matrix result(a.rows_, b.cols_);
        for (size_t row = 0; row < a.rows_; ++row) {
            for (size_t shared = 0; shared < a.cols_; ++shared) {
                const float left = a(row, shared);
                for (size_t col = 0; col < b.cols_; ++col)
                    result(row, col) += left * b(shared, col);
            }
        }
        return result;
    }

    /** @brief Returns a matrix with its rows and columns exchanged. */
    static Matrix Transpose(const Matrix& input) {
        Matrix result(input.cols_, input.rows_);
        for (size_t row = 0; row < input.rows_; ++row) {
            for (size_t col = 0; col < input.cols_; ++col)
                result(col, row) = input(row, col);
        }
        return result;
    }

private:
    size_t rows_ = 0;
    size_t cols_ = 0;
    std::vector<float> data_;
};

} // namespace NoJob::Neural
