#!/usr/bin/env python3
"""生データ直接処理とRawTreeWriter経由処理を比較する（小さいETCCラン用）。

python3 scripts/check/check_raw_replay.py --data-dir <storage0/run> --extra-data-dir <storage1/run>
出力は指定した作業先へ保存。デフォルトは一時ディレクトリ、先頭200イベント。
"""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import numpy as np
import ROOT
ROOT.gROOT.SetBatch(True)
REPO = Path(__file__).resolve().parents[2]


def objects(directory, prefix=''):
    for name in dict.fromkeys(k.GetName() for k in directory.GetListOfKeys()):
        obj = directory.Get(name)
        path = prefix + name
        if obj.InheritsFrom('TDirectory'):
            yield from objects(obj, path + '/')
        elif obj.InheritsFrom('TTree') or obj.InheritsFrom('TH1'):
            yield path, obj


def values(value):
    try:
        return np.asarray(list(value))
    except TypeError:
        return np.asarray(value)


def compare(first, second):
    a, b = ROOT.TFile.Open(str(first)), ROOT.TFile.Open(str(second))
    assert a and b and not a.IsZombie() and not b.IsZombie()
    try:
        paths = {p for p, _ in objects(a)}
        assert paths == {p for p, _ in objects(b)}, 'different object paths'
        for path, x in objects(a):
            y = b.Get(path)
            assert x.ClassName() == y.ClassName(), path
            if x.InheritsFrom('TTree'):
                assert x.GetEntries() == y.GetEntries(), path
                branches = [v.GetName() for v in x.GetListOfBranches()]
                assert branches == [v.GetName() for v in y.GetListOfBranches()], path
                for i in range(x.GetEntries()):
                    x.GetEntry(i); y.GetEntry(i)
                    for name in branches:
                        assert np.array_equal(values(getattr(x, name)), values(getattr(y, name)), equal_nan=True), (path, i, name)
                print(path, x.GetEntries(), 'entries: identical')
            else:
                assert x.GetNcells() == y.GetNcells() and x.GetEntries() == y.GetEntries(), path
                for i in range(x.GetNcells()):
                    assert x.GetBinContent(i) == y.GetBinContent(i) and x.GetBinError(i) == y.GetBinError(i), (path, i)
        print(len(paths), 'ROOT objects: identical')
    finally:
        a.Close(); b.Close()


def mismatch_psa_ids(source, target):
    """Create an input with a PSA event ID mismatch halfway through the file."""
    from array import array
    first = ROOT.TFile.Open(str(source))
    second = ROOT.TFile.Open(str(target), 'RECREATE')
    try:
        for name in ('TPC_RawTreeWriter', 'PSA_RawTreeWriter'):
            directory = second.mkdir(name)
            for tree_name in ('run_info', 'raw'):
                tree = first.Get(name + '/' + tree_name)
                directory.cd()
                change = name == 'PSA_RawTreeWriter' and tree_name == 'raw'
                if change:
                    tree.SetBranchStatus('sevt_id', 0)
                copied = tree.CloneTree(-1, 'fast')
                if change:
                    tree.SetBranchStatus('sevt_id', 1)
                    value = array('I', [0])
                    branch = copied.Branch('sevt_id', value, 'sevt_id/i')
                    for i in range(tree.GetEntries()):
                        tree.GetEntry(i)
                        value[0] = int(tree.sevt_id) + (1 if i == tree.GetEntries() // 2 else 0)
                        branch.Fill()
                copied.Write()
    finally:
        second.Close(); first.Close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', required=True)
    parser.add_argument('--extra-data-dir', action='append', default=[])
    parser.add_argument('-n', type=int, default=200, help='イベント数 [events]')
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    work = args.work_dir or Path(tempfile.mkdtemp(prefix='ericanext-replay-'))
    work.mkdir(parents=True, exist_ok=True)
    data = ['--data-dir', args.data_dir]
    for extra in args.extra_data_dir:
        data += ['--extra-data-dir', extra]

    def run(config, name, options, expected=0):
        output = work / (name + '.root')
        cmd = [sys.executable, str(REPO / 'run_script/run_chain.py'), config, '-n', str(args.n), '-o', str(output), *options]
        with (work / (name + '.log')).open('w') as log:
            result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
        if (result.returncode == 0) != (expected == 0):
            raise RuntimeError(f'{name}: unexpected exit {result.returncode}, see {work / (name + ".log")}')
        if expected != 0:
            assert not output.exists() and not output.with_suffix('.root.part').exists(), name
        return output

    raw = run('smile3_etcc_raw', 'raw', data)
    direct = run('smile3_reconstruct', 'direct', data)
    replay = run('smile3_reconstruct', 'replay', ['--root-input', str(raw)])
    compare(direct, replay)
    copied = run('smile3_etcc_raw', 'copied', ['--root-input', str(raw)])
    compare(raw, copied)
    # A TPC-only raw file cannot supply PSA waveforms to ETCC reconstruction.
    tpc_only = run('smile3_tpc_raw', 'tpc_only', ['--root-input', str(raw)])
    run('smile3_reconstruct', 'missing_psa', ['--root-input', str(tpc_only)], expected=1)
    incomplete = work / 'nohits.root'
    first = ROOT.TFile.Open(str(tpc_only))
    second = ROOT.TFile.Open(str(incomplete), 'RECREATE')
    directory = second.mkdir('TPC_RawTreeWriter')
    for tree_name in ('run_info', 'raw'):
        tree = first.Get('TPC_RawTreeWriter/' + tree_name)
        if tree_name == 'raw':
            tree.SetBranchStatus('hit_a_strip', 0)
        directory.cd()
        tree.CloneTree(-1, 'fast').Write()
    second.Close(); first.Close()
    run('smile3_tpc_only', 'nohits_rejected', ['--root-input', str(incomplete)], expected=1)
    mismatched = work / 'mismatched.root'
    mismatch_psa_ids(raw, mismatched)
    run('smile3_reconstruct', 'mismatch_rejected', ['--root-input', str(mismatched)], expected=1)
    print('PASS: reconstruction and raw round-trip agree; incomplete inputs rejected. Logs:', work)


if __name__ == '__main__':
    main()
