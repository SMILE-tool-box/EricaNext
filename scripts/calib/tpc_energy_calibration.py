"""TPC全体スペクトルと二次エネルギー補正の数値処理。"""
import json
from pathlib import Path
import numpy as np
from scipy.optimize import curve_fit


def gaussian_linear(x,amplitude,mean,sigma,b0,b1):
    return amplitude*np.exp(-.5*((x-mean)/sigma)**2)+b0+b1*(x-mean)


def fit_peak(edges,counts,window,chi2ndf_max=5.,significance_min=3.):
    edges=np.asarray(edges,float);counts=np.asarray(counts,float);centres=(edges[:-1]+edges[1:])/2
    lo,hi=map(float,window)
    if not np.isfinite([lo,hi]).all() or hi<=lo:raise ValueError('fit範囲が不正です')
    sel=(centres>=lo)&(centres<=hi);x=centres[sel];y=counts[sel]
    if len(x)<=6 or y.sum()<30:raise ValueError('ピークfitに十分なビン・統計がありません')
    dx=float(np.median(np.diff(centres)));bg=float(np.quantile(y,.25))
    p0=[max(1.,float(y.max()-bg)),float(x[np.argmax(y)]),(hi-lo)/7,bg,0.]
    p,cov=curve_fit(gaussian_linear,x,y,p0=p0,sigma=np.sqrt(np.maximum(y,1)),absolute_sigma=True,
        bounds=([0,lo,dx/2,-np.inf,-np.inf],[np.inf,hi,hi-lo,np.inf,np.inf]),maxfev=50000)
    errors=np.sqrt(np.maximum(0,np.diag(cov)));model=gaussian_linear(x,*p)
    chi2=float(np.sum((y-model)**2/np.maximum(y,1)));ndf=len(x)-5
    reasons=[]
    if not np.isfinite(cov).all() or errors[1]<=0:reasons.append('invalid mean uncertainty')
    if chi2/ndf>chi2ndf_max:reasons.append('chi2/ndf')
    if errors[0]<=0 or p[0]/errors[0]<significance_min:reasons.append('amplitude significance')
    if p[1]-lo<2*p[2] or hi-p[1]<2*p[2]:reasons.append('fit window truncates the peak')
    if np.any(model<0):reasons.append('negative model expectation')
    return {'mean':float(p[1]),'mean_error':float(errors[1]),'sigma':float(p[2]),
            'parameters':p.tolist(),'covariance':cov.tolist(),'window':[lo,hi],
            'chi2':chi2,'ndf':ndf,'chi2ndf':chi2/ndf,'accepted':not reasons,'reasons':reasons}


def quadratic_calibration(means,errors,references):
    x=np.asarray(means,float);s=np.asarray(errors,float);y=np.asarray(references,float)
    if x.shape!=(3,) or s.shape!=(3,) or y.shape!=(3,) or not np.isfinite(np.r_[x,s,y]).all():
        raise ValueError('有限な3個の中心値・標準誤差・基準エネルギーが必要です')
    if np.any(np.diff(x)<=0) or np.any(np.diff(y)<=0) or np.any(s<=0):
        raise ValueError('中心値と基準値は昇順、標準誤差は正で指定してください')
    v=np.column_stack([np.ones(3),x,x*x]);inverse=np.linalg.inv(v);a=np.linalg.solve(v,y)
    slopes=a[1]+2*a[2]*x
    if np.any(slopes<=0):raise ValueError('ピーク範囲で単調増加する二次補正になりません')
    j=-inverse*slopes[np.newaxis,:];cov=j@np.diag(s*s)@j.T
    return {'format':'TPC_ENERGY_QUADRATIC_V1','coefficients':a.tolist(),'range_keV':[float(x[0]),float(x[-1])],
            'outside':'tangent','covariance':cov.tolist(),'coefficient_units':['keV','1','1/keV'],
            'means_keV':x.tolist(),'mean_errors_keV':s.tolist(),'references_keV':y.tolist(),
            'uncertainty_assumptions':'independent peak means; fixed reference energies; first-order statistical propagation',
            'ndf':0}


def evaluate(energy,cal):
    x=np.asarray(energy,float);a0,a1,a2=cal['coefficients'];lo,hi=cal['range_keV']
    clipped=np.clip(x,lo,hi);f=a0+a1*clipped+a2*clipped*clipped
    return np.maximum(0.,f+(a1+2*a2*clipped)*(x-clipped))


def write_correction(path,cal,overwrite=False):
    path=Path(path)
    if path.exists() and not overwrite:raise FileExistsError(path)
    a=cal['coefficients'];lo,hi=cal['range_keV']
    # Round-trip through the same validation used for chain inputs.
    if not np.isfinite(list(a)+[lo,hi]).all() or hi<=lo or min(a[1]+2*a[2]*lo,a[1]+2*a[2]*hi)<=0:raise ValueError('補正が単調増加ではありません')
    text='TPC_ENERGY_QUADRATIC_V1\ncoefficients '+ ' '.join(format(v,'.17g') for v in a)+'\nrange '+format(lo,'.17g')+' '+format(hi,'.17g')+'\noutside tangent\n'
    path.parent.mkdir(parents=True,exist_ok=True);tmp=path.with_name(path.name+'.part');tmp.write_text(text);tmp.replace(path)
    path.with_suffix('.json').write_text(json.dumps(cal,ensure_ascii=False,indent=2)+'\n')


def read_correction(path):
    lines=Path(path).read_text().splitlines()
    if len(lines)!=4 or lines[0]!='TPC_ENERGY_QUADRATIC_V1' or lines[3]!='outside tangent':raise ValueError('補正ファイル形式が不正です')
    a=lines[1].split();r=lines[2].split()
    if len(a)!=4 or a[0]!='coefficients' or len(r)!=3 or r[0]!='range':raise ValueError('補正ファイル形式が不正です')
    coeff=list(map(float,a[1:]));lo,hi=map(float,r[1:])
    if not np.isfinite(coeff+[lo,hi]).all() or hi<=lo or min(coeff[1]+2*coeff[2]*lo,coeff[1]+2*coeff[2]*hi)<=0:raise ValueError('補正ファイルの値が不正です')
    return {'coefficients':coeff,'range_keV':[lo,hi],'outside':'tangent'}
