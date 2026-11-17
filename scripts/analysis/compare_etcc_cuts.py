"""Compare ETCC energy spectra and sky backprojections with explicit event cuts."""
import argparse
import glob
import json
from pathlib import Path

import numpy as np
import ROOT
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle
from matplotlib.colors import LogNorm

ROOT.gROOT.SetBatch(True)


def read_events(files, require_tracks=True):
    """Join tracks within each file, so trigger counters from different runs cannot mix."""
    pieces = []
    audit = []
    scalar = ['evt_id', 'Eg', 'Ke', 'E0', 'cos_phi', 'has_track',
              'cos_alpha_kin', 'cos_alpha_geo', 'delta_cos_alpha']
    for path in files:
        f = ROOT.TFile.Open(path)
        if not f or f.IsZombie():
            raise OSError(path)
        try:
            tree = f.Get('Reconstruct/single_hit')
            if not tree:
                raise ValueError(f'{path}: missing Reconstruct/single_hit')
            tracks = f.Get('TOT_Skewness/tot_skewness')
            for name in (['evt_id', 'Len', 'track_min', 'track_max'] if require_tracks else []):
                if not tracks or not tracks.GetBranch(name):
                    raise ValueError(f'{path}: missing track branch {name}; replay reconstruction first')
            frame = ROOT.RDataFrame(tree)
            cols = list(scalar)
            for vector in ['Pe', 'Pg', 'Vi']:
                for j in range(3):
                    name = f'{vector}{j}'
                    frame = frame.Define(name, f'{vector}[{j}]')
                    cols.append(name)
            d = frame.AsNumpy(cols)
            if not require_tracks:
                pieces.append(d)
                audit.append({'file': path, 'single_hit': len(d['E0'])})
                print(path, len(d['E0']), 'single_hit', flush=True)
                continue
            frame = ROOT.RDataFrame(tracks)
            cols = ['evt_id', 'Len']
            for vector in ['track_min', 'track_max', 'Pe']:
                for j in range(3):
                    name = f'{vector}{j}'
                    frame = frame.Define(name, f'{vector}[{j}]')
                    cols.append(name)
            t = frame.AsNumpy(cols)
            order = np.argsort(t['evt_id'])
            ids = t['evt_id'][order]
            if len(ids) == 0 or np.any(ids[1:] == ids[:-1]):
                raise ValueError(f'{path}: empty or duplicate track event IDs')
            k = np.searchsorted(ids, d['evt_id'])
            k = np.minimum(k, len(ids) - 1)
            found = ids[k] == d['evt_id']
            if not np.all(found):
                raise ValueError(f'{path}: {np.sum(~found)} unmatched reconstructed events')
            for name in cols[1:]:
                values = t[name][order[k]]
                if name.startswith('Pe'):
                    valid = d['has_track'] == 1
                    if not np.allclose(values[valid], d[name][valid], rtol=0, atol=1e-7):
                        raise ValueError(f'{path}: Pe mismatch in event ID join')
                else:
                    d[name] = values
            pieces.append(d)
            audit.append({'file': path, 'single_hit': len(d['E0']), 'tracks': len(ids)})
            print(path, len(d['E0']), 'single_hit; join OK', flush=True)
        finally:
            f.Close()
    return {key: np.concatenate([p[key] for p in pieces]) for key in pieces[0]}, audit


def masks(d, scale, margin, alpha):
    track = (d['has_track'] == 1) & (d['Ke'] > 0)
    for name in ['E0', 'Ke', 'cos_phi', 'delta_cos_alpha'] + [f'{v}{j}' for v in ['Pe', 'Pg', 'Vi'] for j in range(3)]:
        track &= np.isfinite(d[name])
    fid = np.ones(len(track), dtype=bool)
    for j in range(3):
        low, high = (-153.6, 153.6) if j < 2 else (10., 317.)
        fid &= (d[f'track_min{j}'] > low + margin) & (d[f'track_max{j}'] < high - margin)
    ke = np.maximum(d['Ke'], 0)
    lower = scale * (913.9 * (ke / 1000)**2 + 2.2)
    upper = scale * (1975.9 * (ke / 1000)**1.4 + 7.0)
    dedx = (d['Len'] > lower) & (d['Len'] < upper) & (d['Len'] < scale * 270)
    both = track & fid & dedx
    return [('Track, no additional cut', track), ('Fiducial only', track & fid),
            ('dE/dx only', track & dedx), ('Fiducial + dE/dx', both),
            (f'Fiducial + dE/dx + |delta cos alpha| < {alpha:g}', both & (np.abs(d['delta_cos_alpha']) < alpha))]


def spectra(d, stages, out, label):
    colors = plt.cm.tab10(np.arange(len(stages)))
    fig, axs = plt.subplots(1, 3, figsize=(18, 5.8))
    for ax, key, high in zip(axs, ['Eg', 'Ke', 'E0'], [1500, 600, 1500]):
        edges = np.arange(0, high + 5, 5)
        h, _ = np.histogram(d[key], edges)
        ax.stairs(h, edges, color='0.65', ls=':', label=f'All single_hit (N={len(d[key]):,})')
        for (name, mask), color in zip(stages, colors):
            ax.stairs(np.histogram(d[key][mask], edges)[0], edges, color=color,
                      label=f'{name} (N={mask.sum():,})')
        ax.set(xlabel=f'{key} [keV]', ylabel='Events / 5 keV', yscale='log', ylim=(0.7, None), xlim=(0, high))
        ax.grid(alpha=.25)
        if key != 'Ke':
            ax.axvline(661.66, color='0.5', ls='--', lw=.8)
    axs[0].legend(fontsize=7.5)
    fig.suptitle(f'{label}: energy spectra; no Eg cut, no E0 window; absolute counts')
    fig.tight_layout()
    fig.savefig(out / 'energy_cut_comparison.png', dpi=160)
    plt.close(fig)




def dedx_plot(d, stages, out, args):
    """Show the Ke-length selection before/after cuts with shared count scales."""
    panels = [stages[0], stages[1], stages[3]]
    window = (d['E0'] >= args.emin) & (d['E0'] <= args.emax) & (np.abs(d['cos_phi']) <= 1)
    xe = np.linspace(0., 600., 151)
    ye = np.linspace(0., 300., 101)
    histograms, audit = [], []
    for windowed in (False, True):
        row = []
        for name, mask in panels:
            selected = mask & np.isfinite(d['Len'])
            if windowed:
                selected &= window
            h = np.histogram2d(d['Ke'][selected], d['Len'][selected], bins=(xe, ye))[0]
            row.append(h)
            audit.append({'cut': name, 'E0_window': windowed, 'selected_events': int(selected.sum()),
                          'displayed_events': int(h.sum())})
        histograms.append(row)
    vmax = max(float(h.max()) for row in histograms for h in row)
    norm = LogNorm(vmin=1., vmax=max(2., vmax))
    fig, axs = plt.subplots(2, 3, figsize=(18, 10), sharex=True, sharey=True, constrained_layout=True)
    energy = np.linspace(0., 600., 1201)
    for r, row in enumerate(histograms):
        for c, h in enumerate(row):
            ax = axs[r, c]
            im = ax.pcolormesh(xe, ye, np.ma.masked_less(h.T, 1), cmap='turbo', norm=norm, shading='flat')
            for scale, style, colour, label in [(1., ':', '0.5', 'SMILE-2+ band'),
                                                (args.len_scale, '-', 'black', f'Used band x {args.len_scale:g}')]:
                lower = scale * (913.9 * (energy/1000)**2 + 2.2)
                upper = scale * (1975.9 * (energy/1000)**1.4 + 7.)
                ax.plot(energy, lower, style, color=colour, lw=1.2, label=label)
                ax.plot(energy, upper, style, color=colour, lw=1.2)
            ax.axhline(270*args.len_scale, color='black', ls='--', lw=1, label='Used length maximum')
            entry = audit[3*r+c]
            ax.set(title=f"{panels[c][0]}\nN={entry['selected_events']:,}; shown={entry['displayed_events']:,}",
                   xlabel='Ke [keV]', ylabel='Track length [mm]', xlim=(0, 600), ylim=(0, 300))
            ax.text(.97, .97, 'No E0 window' if r == 0 else f'E0 {args.emin:g}-{args.emax:g} keV; |cos phi| <= 1',
                    ha='right', va='top', transform=ax.transAxes, fontsize=9,
                    bbox={'facecolor': 'white', 'alpha': .8, 'edgecolor': 'none'})
    axs[0, 0].legend(loc='lower right', fontsize=8, framealpha=.9)
    fig.colorbar(im, ax=axs.ravel().tolist(), label='Events / (4 keV x 3 mm)', fraction=.025, pad=.02)
    fig.suptitle(f'{args.label}: Ke vs track length (dE/dx selection); no Eg cut; common logarithmic count scale')
    fig.savefig(out / 'dedx_cut_comparison.png', dpi=150)
    plt.close(fig)
    return {'Ke_range_keV': [0., 600.], 'Len_range_mm': [0., 300.],
            'Ke_bin_width_keV': 4., 'Len_bin_width_mm': 3., 'panels': audit}


def sky_radius(theta_deg, projection):
    """Zenith angle or Lambert equal-area radius in degree-equivalent units."""
    if projection == 'lambert':
        return np.degrees(2 * np.sin(np.radians(theta_deg) / 2))
    return np.asarray(theta_deg)


def maps(d, stages, out, args):
    pe = np.column_stack([d[f'Pe{j}'] for j in range(3)])
    pg = np.column_stack([d[f'Pg{j}'] for j in range(3)])
    vi = np.column_stack([d[f'Vi{j}'] for j in range(3)])
    window = (d['E0'] >= args.emin) & (d['E0'] <= args.emax) & (np.abs(d['cos_phi']) <= 1)
    selected = [m & window for _, m in stages]
    projection = args.projection
    nb = args.bins
    half = float(sky_radius(90., projection))
    cones = [np.zeros((nb, nb)) for _ in stages]
    idx = np.flatnonzero(selected[0])
    psi = np.linspace(0, 2*np.pi, 360, endpoint=False)
    for start in range(0, len(idx), 1000):
        ii = idx[start:start+1000]
        axis = pe[ii] - pg[ii]
        axis /= np.linalg.norm(axis, axis=1)[:, None]
        u = np.cross(axis, [0, 0, 1.])
        u[np.linalg.norm(u, axis=1) < 1e-9] = [1, 0, 0]
        u /= np.linalg.norm(u, axis=1)[:, None]
        v = np.cross(axis, u)
        c = d['cos_phi'][ii]
        dirs = axis[:, None, :] * c[:, None, None] + np.sqrt(1-c*c)[:, None, None] * (u[:, None, :] * np.cos(psi)[None, :, None] + v[:, None, :] * np.sin(psi)[None, :, None])
        theta = np.degrees(np.arccos(np.clip(-dirs[:, :, 2], -1, 1)))
        phi = np.arctan2(dirs[:, :, 1], dirs[:, :, 0])
        radius = sky_radius(theta, projection)
        x, y = radius*np.cos(phi), radius*np.sin(phi)
        for j, mask in enumerate(selected):
            keep = mask[ii, None] & (theta <= 90.)
            cones[j] += np.histogram2d(x[keep], y[keep], bins=nb, range=[[-half, half]]*2)[0] / len(psi)
    results, counts = [], []
    src = np.array(args.src)
    centre = np.array([0., 0., 160.])
    source_direction = src-centre
    source_direction /= np.linalg.norm(source_direction)
    th0 = np.degrees(np.arccos(-source_direction[2]))
    ph0 = np.arctan2(source_direction[1], source_direction[0])
    source_radius = float(sky_radius(th0, projection))
    for j, ((name, _), mask) in enumerate(zip(stages, selected)):
        sd = -vi[mask]
        theta = np.degrees(np.arccos(np.clip(-sd[:, 2], -1, 1)))
        phi = np.arctan2(sd[:, 1], sd[:, 0])
        keep = theta <= 90.
        radius = sky_radius(theta, projection)
        image = np.histogram2d((radius*np.cos(phi))[keep], (radius*np.sin(phi))[keep], bins=nb, range=[[-half, half]]*2)[0]
        g = pg[mask]-pe[mask]
        g /= np.linalg.norm(g, axis=1)[:, None]
        incoming = pe[mask]-src
        incoming /= np.linalg.norm(incoming, axis=1)[:, None]
        arm = np.degrees(np.arccos(np.clip(np.sum(g*incoming, axis=1), -1, 1))-np.arccos(d['cos_phi'][mask]))
        results.append((name, cones[j], image, arm))
        counts.append({'cut': name, 'spectrum_events': int(stages[j][1].sum()), 'E0_window_events': int(mask.sum()),
                       'upper_hemisphere_events': int(keep.sum()), 'ARM_median_deg': float(np.median(arm)) if len(arm) else None,
                       'ARM_IQR_deg': float(np.subtract(*np.percentile(arm, [75, 25]))) if len(arm) else None})
    vmax_cone = max(float(r[1].max()) for r in results) or 1
    vmax_etcc = max(float(r[2].max()) for r in results) or 1
    # Equal colour limits preserve absolute count comparisons across cuts.
    def draw(axs, result, common_scale=True):
        name, cone, image, arm = result
        for ax, data, title, vmax in zip(axs[:2], [cone, image], [f'Compton cones; N={len(arm):,}', f'ETCC; N={int(image.sum()):,}'], [vmax_cone, vmax_etcc]):
            if not common_scale:
                vmax = float(data.max()) or 1
            im = ax.imshow(data.T, origin='lower', extent=(-half, half, -half, half), cmap='turbo', vmin=0, vmax=vmax)
            ax.plot(source_radius*np.cos(ph0), source_radius*np.sin(ph0), 'k+', ms=12, mew=2)
            for r in [30, 60, 90]:
                ax.add_patch(Circle((0, 0), float(sky_radius(r, projection)), fill=False, color='k', lw=.5, ls=':'))
            axis_label = 'Lambert' if projection == 'lambert' else 'theta'
            units = 'deg-equivalent' if projection == 'lambert' else 'deg'
            ax.set(title=title, xlabel=f'{axis_label} X [{units}] (+X = E)',
                   ylabel=f'{axis_label} Y [{units}] (+Y = S)')
            plt.colorbar(im, ax=ax, shrink=.8, label='Events / bin')
        ax = axs[2]
        ax.hist(arm, bins=np.arange(-180, 181, 1), histtype='step', color='#2467df')
        ax.set(xlim=(-90, 90), xlabel='ARM [deg]', ylabel='Events / deg', title=f'ARM; N={len(arm):,}')
        ax.axvline(0, color='0.5', ls=':')
        ax.grid(alpha=.3)
        if len(arm):
            q = np.percentile(arm, [25, 50, 75])
            ax.text(.02, .95, f'median {q[1]:.1f} deg; IQR {q[2]-q[0]:.1f} deg', va='top', transform=ax.transAxes)
    for j, result in enumerate(results):
        fig, axs = plt.subplots(1, 3, figsize=(18, 5.3))
        draw(axs, result, common_scale=False)
        fig.suptitle(f'{args.label}; {projection} projection; E0 {args.emin:g}-{args.emax:g} keV; no Eg cut\n{result[0]}; source {tuple(args.src)} mm; individual colour limits')
        fig.tight_layout()
        fig.savefig(out / f'backprojection_{j:02d}.png', dpi=150)
        plt.close(fig)
    fig, axs = plt.subplots(len(results), 3, figsize=(18, 5*len(results)), squeeze=False)
    for row, result in zip(axs, results):
        draw(row, result)
        row[0].set_title(result[0] + '\nCompton cones', fontsize=10)
    fig.suptitle(f'{args.label}; {projection} projection; E0 {args.emin:g}-{args.emax:g} keV; no Eg cut; common colour limits', y=.999)
    fig.tight_layout(rect=(0, 0, 1, .99))
    fig.savefig(out / 'backprojection_cut_comparison.png', dpi=130)
    plt.close(fig)
    fig, axs = plt.subplots(len(results), 3, figsize=(18, 5*len(results)), squeeze=False)
    for j, (row, result) in enumerate(zip(axs, results)):
        draw(row, result, common_scale=False)
        row[0].set_title(result[0] + '\nCompton cones', fontsize=10)
        row[1].set_title(f'ETCC; N={counts[j]["upper_hemisphere_events"]:,}', fontsize=10)
    fig.suptitle(f'{args.label}; {projection} projection; E0 {args.emin:g}-{args.emax:g} keV; {nb} x {nb} bins; individual colour limits', y=.999)
    fig.tight_layout(rect=(0, 0, 1, .99))
    fig.savefig(out / 'backprojection_cut_comparison_autoscale.png', dpi=130)
    plt.close(fig)
    return counts


def main():
    ap = argparse.ArgumentParser(__doc__)
    ap.add_argument('files', nargs='+', help='Input reconstructed ROOT files or quoted glob patterns')
    ap.add_argument('--baseline-only', action='store_true', help='Only no-additional-cut maps; supports older ROOT without track bounds')
    ap.add_argument('--out', required=True, type=Path)
    ap.add_argument('--src', required=True, type=float, nargs=3, help='Measured source position [mm]')
    ap.add_argument('--label', default='ETCC')
    ap.add_argument('--emin', type=float, default=630., help='Map E0 minimum [keV]; spectra are not windowed')
    ap.add_argument('--emax', type=float, default=700., help='Map E0 maximum [keV]')
    ap.add_argument('--alpha', type=float, default=.5, help='Maximum absolute delta cos alpha [dimensionless]')
    ap.add_argument('--margin', type=float, default=5., help='Fiducial margin from outer walls and drift ends [mm]')
    ap.add_argument('--len-scale', type=float, default=1., help='SMILE-2+ dE/dx length scale; default 1 for the same 2 atm pressure')
    ap.add_argument('--projection', choices=('zenith', 'lambert'), default='zenith', help='Sky map: zenith-angle radius or Lambert equal-area')
    ap.add_argument('--bins', type=int, default=45, help='Sky map bins per axis')
    args = ap.parse_args()
    files = sorted(set(f for pattern in args.files for f in glob.glob(pattern)))
    if not files:
        ap.error('No input files')
    args.out.mkdir(parents=True, exist_ok=True)
    d, audit = read_events(files, require_tracks=not args.baseline_only)
    if args.baseline_only:
        valid = (d['has_track'] == 1) & (d['Ke'] > 0)
        for key in ['E0', 'cos_phi'] + [f'{v}{j}' for v in ['Pe', 'Pg', 'Vi'] for j in range(3)]:
            valid &= np.isfinite(d[key])
        stages = [('Track, no additional cut', valid)]
    else:
        stages = masks(d, args.len_scale, args.margin, args.alpha)
    spectra(d, stages, args.out, args.label)
    dedx_audit = None if args.baseline_only else dedx_plot(d, stages, args.out, args)
    counts = maps(d, stages, args.out, args)
    metadata = {'arguments': {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
                'dedx_plot': dedx_audit, 'inputs': audit, 'all_single_hit': len(d['E0']), 'cuts': counts,
                'fiducial_mm': {'xy': [-153.6+args.margin, 153.6-args.margin], 'z': [10+args.margin, 317-args.margin]},
                'dedx': 'No additional Ke range cut; s*(913.9*(Ke/1000)^2+2.2) < Len < s*(1975.9*(Ke/1000)^1.4+7); Len < s*270 mm',
                'alpha': 'abs(cos_alpha_geo - cos_alpha_kin) < alpha',
                'notes': ['No additional Eg cut; PSA_Event upstream energy selection is retained.',
                          'Spectra have no E0 window; maps require E0 window and abs(cos_phi)<=1.',
                          'All map comparisons use tracked events and common colour limits.',
                          'TPC and PSA frames are assumed aligned, as in the inherited analysis.',
                          'Both measurements use 2 atm: the default length scale is 1 (SMILE-2+ band and 270 mm maximum); no 5-400 keV selection. The band is not optimized on SMILE-3 data.',
                          'Cones sample 360 azimuths per event, each weight 1/360; hemisphere clipping loses weight.',
                          ('Zenith-radius bins are not equal solid angle.' if args.projection == 'zenith' else 'Lambert bins have equal area; partial boundary-bin areas are not corrected.'),
                          'Maps show raw counts without acceptance correction.']}
    (args.out / 'selection_summary.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print(json.dumps(counts, indent=2), flush=True)


if __name__ == '__main__':
    main()
