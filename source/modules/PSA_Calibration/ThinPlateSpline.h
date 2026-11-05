#ifndef ThinPlateSpline_h
#define ThinPlateSpline_h 1

#include <vector>

// 2D -> 2D thin-plate spline interpolation through given points (no smoothing),
// f(p) = sum_k w_k phi(|p - p_k|) + c0 + c1 x + c2 y, phi(r) = r^2 log r.
// The same interpolant as scipy.interpolate.RBFInterpolator(kernel="thin_plate_spline",
// smoothing=0) used by grid_spect.py and pixel_id.py.
class ThinPlateSpline {
 public:
  // false when the system is singular (e.g. points on a line or duplicated)
  bool Fit(const std::vector<double>& x, const std::vector<double>& y,
           const std::vector<double>& u, const std::vector<double>& v);
  void Eval(double x, double y, double& u, double& v) const;
  bool Ready() const { return !px_.empty(); }

 private:
  std::vector<double> px_, py_, wu_, wv_;
  double cu_[3] = {0, 0, 0}, cv_[3] = {0, 0, 0};
};

#endif
