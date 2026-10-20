#pragma once
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <algorithm>
#include <cmath>
namespace NoJob::Neural {
class Matrix {
public:
 Matrix()=default;
 Matrix(size_t rows,size_t cols,float initial=0):rows_(rows),cols_(cols),data_(rows*cols,initial){if(!rows||!cols)throw std::invalid_argument("Empty matrix dimension");}
 size_t Rows()const{return rows_;} size_t Cols()const{return cols_;}
 float& operator()(size_t r,size_t c){return data_.at(r*cols_+c);} float operator()(size_t r,size_t c)const{return data_.at(r*cols_+c);}
 const std::vector<float>& Data()const{return data_;} std::vector<float>& Data(){return data_;}
 static Matrix Multiply(const Matrix&a,const Matrix&b){if(a.cols_!=b.rows_)throw std::invalid_argument("Matrix multiply dimensions");Matrix out(a.rows_,b.cols_);for(size_t i=0;i<a.rows_;++i)for(size_t k=0;k<a.cols_;++k)for(size_t j=0;j<b.cols_;++j)out(i,j)+=a(i,k)*b(k,j);return out;}
 static Matrix Transpose(const Matrix&a){Matrix out(a.cols_,a.rows_);for(size_t i=0;i<a.rows_;++i)for(size_t j=0;j<a.cols_;++j)out(j,i)=a(i,j);return out;}
private:size_t rows_=0,cols_=0;std::vector<float> data_;
};
}
