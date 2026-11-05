#include "ThinPlateSpline.h"

#include <cmath>
#include <utility>

namespace {
inline double phi(double r2){
  return r2 > 0 ? 0.5 * r2 * std::log(r2) : 0.;   // r^2 log r = r2 log(r2) / 2
}

// Solve A X = B (n x n, two right-hand sides) by LU with partial pivoting. A and B are overwritten.
bool solve(std::vector<std::vector<double>>& A, std::vector<double>& b1, std::vector<double>& b2){
  const int n = (int)A.size();
  for(int k=0; k<n; k++){
    int p = k;
    for(int i=k+1; i<n; i++)  if(std::fabs(A[i][k]) > std::fabs(A[p][k]))  p = i;
    if(std::fabs(A[p][k]) < 1e-300)  return false;
    std::swap(A[k], A[p]);
    std::swap(b1[k], b1[p]);
    std::swap(b2[k], b2[p]);
    for(int i=k+1; i<n; i++){
      double f = A[i][k] / A[k][k];
      if(f == 0)  continue;
      for(int j=k; j<n; j++)  A[i][j] -= f * A[k][j];
      b1[i] -= f * b1[k];
      b2[i] -= f * b2[k];
    }
  }
  for(int i=n-1; i>=0; i--){
    for(int j=i+1; j<n; j++){
      b1[i] -= A[i][j] * b1[j];
      b2[i] -= A[i][j] * b2[j];
    }
    b1[i] /= A[i][i];
    b2[i] /= A[i][i];
  }
  return true;
}
}

bool ThinPlateSpline::Fit(const std::vector<double>& x, const std::vector<double>& y,
                          const std::vector<double>& u, const std::vector<double>& v){
  px_.clear();
  const int n = (int)x.size();
  if(n < 3)  return false;
  const int m = n + 3;
  std::vector<std::vector<double>> A(m, std::vector<double>(m, 0.));
  std::vector<double> bu(m, 0.), bv(m, 0.);
  for(int i=0; i<n; i++){
    for(int j=0; j<n; j++){
      double dx = x[i] - x[j], dy = y[i] - y[j];
      A[i][j] = phi(dx*dx + dy*dy);
    }
    A[i][n] = A[n][i] = 1.;
    A[i][n+1] = A[n+1][i] = x[i];
    A[i][n+2] = A[n+2][i] = y[i];
    bu[i] = u[i];
    bv[i] = v[i];
  }
  if(!solve(A, bu, bv))  return false;
  px_ = x;
  py_ = y;
  wu_.assign(bu.begin(), bu.begin() + n);
  wv_.assign(bv.begin(), bv.begin() + n);
  for(int k=0; k<3; k++){
    cu_[k] = bu[n + k];
    cv_[k] = bv[n + k];
  }
  return true;
}

void ThinPlateSpline::Eval(double x, double y, double& u, double& v) const {
  u = cu_[0] + cu_[1] * x + cu_[2] * y;
  v = cv_[0] + cv_[1] * x + cv_[2] * y;
  for(size_t k=0; k<px_.size(); k++){
    double dx = x - px_[k], dy = y - py_[k];
    double p = phi(dx*dx + dy*dy);
    u += wu_[k] * p;
    v += wv_[k] * p;
  }
}
