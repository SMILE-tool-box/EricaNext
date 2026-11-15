#!/usr/bin/env python3
"""TPC_GainMapノートブックの係数誤差を数値微分・乱数試験で検証する。"""
import ast,json,unittest
from pathlib import Path
import numpy as np

path=Path(__file__).resolve().parents[2]/'tutorials/4_MakeCalibration/TPC_GainMap.ipynb'
nb=json.loads(path.read_text())
source=next(''.join(c['source']) for c in nb['cells'] if 'def two_point_calibration' in ''.join(c['source']))
node=next(n for n in ast.parse(source).body if isinstance(n,ast.FunctionDef) and n.name=='two_point_calibration')
scope={'np':np};exec(compile(ast.Module(body=[node],type_ignores=[]),str(path),'exec'),scope)
calibrate=scope['two_point_calibration']

class CalibrationErrorsTest(unittest.TestCase):
    def setUp(self):
        self.mu=np.array([700.,3000.]);self.err=np.array([5.,20.]);self.energy=np.array([8.04,30.85])
    def test_jacobian_against_numerical_derivative(self):
        ka,kb,u=calibrate(self.mu,self.err,self.energy)
        np.testing.assert_allclose(ka*self.mu+kb,self.energy)
        columns=[]
        for k in range(2):
            step=np.zeros(2);step[k]=.001
            plus=np.array(calibrate(self.mu+step,self.err,self.energy)[:2])
            minus=np.array(calibrate(self.mu-step,self.err,self.energy)[:2])
            columns.append((plus-minus)/.002)
        j=np.column_stack(columns)
        expected=j@np.diag(self.err**2)@j.T
        np.testing.assert_allclose(u['covariance'],expected,rtol=1e-6)
        self.assertLess(u['cov_Ka_Kb'],0)
    def test_monte_carlo(self):
        _,_,u=calibrate(self.mu,self.err,self.energy)
        q=np.random.default_rng(1501).normal(self.mu,self.err,size=(200000,2))
        a=np.diff(self.energy)[0]/(q[:,1]-q[:,0]);b=self.energy[0]-a*q[:,0]
        np.testing.assert_allclose(np.cov(np.stack([a,b])),u['covariance'],rtol=.02)
    def test_energy_error_needs_covariance(self):
        ka,_,u=calibrate(self.mu,self.err,self.energy);cov=np.array(u['covariance'])
        for q,s in zip(self.mu,self.err):
            v=np.array([q,1.])
            self.assertAlmostEqual(float(v@cov@v),(ka*s)**2,places=12)
    def test_zero_errors_and_invalid_inputs(self):
        _,_,u=calibrate(self.mu,[0,0],self.energy)
        self.assertEqual(u['Ka_stat_error'],0);self.assertEqual(u['Kb_stat_error'],0)
        self.assertIsNone(u['corr_Ka_Kb'])
        for mu,err in [([700,700],[1,1]),([700,3000],[-1,1]),([700,np.nan],[1,1])]:
            with self.subTest(mu=mu,err=err):
                with self.assertRaises(ValueError):calibrate(mu,err,self.energy)
if __name__=='__main__':unittest.main()
