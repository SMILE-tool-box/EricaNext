import unittest,sys,tempfile
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'calib'))
from tpc_energy_calibration import quadratic_calibration,evaluate,write_correction,read_correction,fit_peak,gaussian_linear
class TestEnergyCalibration(unittest.TestCase):
    def test_identity(self):
        c=quadratic_calibration([8,30,80],[.03,.1,.3],[8,30,80])
        np.testing.assert_allclose(evaluate(np.array([0,5,8,30,80,150]),c),[0,5,8,30,80,150],atol=1e-11)
    def test_covariance(self):
        x=np.array([8.,30.,75.]);s=np.array([.01,.02,.05]);y=np.array([8.04,30.85,80.9979]);c=quadratic_calibration(x,s,y)
        rng=np.random.default_rng(12);draws=rng.normal(x,s,(20000,3))
        v=np.stack([np.ones_like(draws),draws,draws**2],axis=2)
        a=np.linalg.solve(v,np.broadcast_to(y,(len(draws),3))[...,None])[...,0]
        np.testing.assert_allclose(np.diag(np.cov(a,rowvar=False)),np.diag(c['covariance']),rtol=.05)
        np.testing.assert_allclose(evaluate(x,c),y,atol=1e-11)
    def test_roundtrip_and_tangent(self):
        c=quadratic_calibration([8,30,75],[.1,.1,.1],[8.04,30.85,80.9979])
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'cal.dat';write_correction(p,c);r=read_correction(p)
            xx=np.linspace(-5,200,1000);np.testing.assert_allclose(evaluate(xx,r),evaluate(xx,c));self.assertTrue(np.all(np.diff(evaluate(xx,r))>=0))
    def test_fit(self):
        edges=np.linspace(0,120,601);x=(edges[:-1]+edges[1:])/2
        y=gaussian_linear(x,1000,30,1.5,20,0)
        r=fit_peak(edges,y,[22,38]);self.assertTrue(r['accepted']);self.assertAlmostEqual(r['mean'],30,places=5)
    def test_invalid(self):
        for x,y in [([8,8,80],[8,30,80]),([8,30,80],[8,80,30])]:
            with self.assertRaises(ValueError):quadratic_calibration(x,[.1]*3,y)
if __name__=='__main__':unittest.main()
