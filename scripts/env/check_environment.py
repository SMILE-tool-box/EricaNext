#!/usr/bin/env python3
"""EricaNextのビルド・実行・ノートブック環境を確認する（標準ライブラリのみ）。"""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]


def execute(args, timeout=30):
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        output = (p.stdout + '\n' + p.stderr).strip()
        return p.returncode == 0, output[-1500:]
    except (OSError, subprocess.TimeoutExpired) as exc:
        return False, str(exc)


def probe(python, modules, local=False):
    source = "import sys, json, importlib, sysconfig\n"
    if local:
        source += 'sys.path.insert(0, ' + repr(str(REPO / 'install/lib/python')) + ')\n'
    source += "print('PYTHON_INFO=' + json.dumps({'executable':sys.executable,'version':sys.version.split()[0], 'include':sysconfig.get_path('include')}))\n"
    for name in modules:
        source += 'm = importlib.import_module(' + repr(name) + ')\n'
        if name == 'ROOT':
            source += "assert hasattr(m, 'RDataFrame'), 'ROOT.RDataFrame がありません'\n"
        if name == 'EricaPy':
            source += "assert all(hasattr(m, n) for n in ['ReadData','RawTreeWriter_sm3','TOT_Skewness']), 'SMILE-3モジュールがありません'\n"
        source += "print('MODULE=' + json.dumps({'name':" + repr(name) + ", 'path':getattr(m,'__file__','built-in')}))\n"
    return execute([python, '-B', '-c', source])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--mode', choices=['build','run','notebook','all'], default='all',
                    help='確認対象。初回ビルド前はbuild、ビルド後はall（既定）')
    ap.add_argument('--python', default=os.environ.get('PYTHON', sys.executable),
                    help='ANLNext/EricaPyのビルド・チェーン実行に使うPythonのパス')
    ap.add_argument('--analysis-python', help='較正・ノートブック用Pythonのパス（省略時は--pythonと同じ）')
    args = ap.parse_args()
    analysis = args.analysis_python or args.python
    failures = []

    def check(label, ok, detail, hint):
        print(('OK  ' if ok else 'NG  ') + label)
        if detail:
            for line in detail.splitlines(): print('    ' + line)
        if not ok:
            failures.append(label)
            print('    対処: ' + hint)

    print('リポジトリ:', REPO)
    print('確認対象:', args.mode)
    print('チェーン用Python:', args.python)
    print('解析用Python:', analysis)
    print('データの読み込み・ビルド・パッケージのインストールは行いません。\n')
    if args.mode in ['build','all']:
        for tool, flag in [('cmake','--version'),('make','--version'),('swig','-version'),('root-config','--version')]:
            path = shutil.which(tool)
            ok, msg = execute([path,flag]) if path else (False,'PATH上にありません')
            check(tool, ok, (path+'\n' if path else '')+'\n'.join(msg.splitlines()[:3]),
                  tool+'をインストールし、その環境を有効にしてください。')
        compiler = shlex.split(os.environ.get('CXX','c++'))
        try:
            p = subprocess.run(compiler+['-std=c++17','-fsyntax-only','-x','c++','-'],
                               input='#include <optional>\nint main(){ std::optional<int> x=1; return *x; }\n',
                               capture_output=True,text=True,timeout=30)
            ok, msg = p.returncode==0, (p.stdout+p.stderr).strip()
        except (OSError,subprocess.TimeoutExpired) as exc:
            ok,msg=False,str(exc)
        check('C++17コンパイラ',ok,' '.join(compiler)+'\n'+msg,'C++17対応コンパイラを用意し、必要ならCXXを指定してください。')
        prefix = os.environ.get('ANLNEXT_INSTALL')
        check('ANLNEXT_INSTALLのヘッダー', bool(prefix and (Path(prefix)/'include/anlnext/BasicModule.hh').is_file()),
              prefix or '未設定', 'ANLNextをインストールし、export ANLNEXT_INSTALL=/実際のインストール先 を設定してください。')
        ok,msg=probe(args.python,[])
        info = next((json.loads(l.split('=',1)[1]) for l in msg.splitlines() if l.startswith('PYTHON_INFO=')),{})
        header=Path(info.get('include','/nonexistent'))/'Python.h'
        check('Python開発ヘッダー',ok and header.is_file(),str(header),'選択したPythonと同じ版の開発ヘッダーを用意してください。')
        print('INFO Boost >= 1.80 の検出・リンクと依存関係の版の整合は、実際のCMakeビルドで確認します。')

    if args.mode in ['build','run','all']:
        modules=['anlnext'] if args.mode=='build' else ['anlnext','EricaPy']
        ok,msg=probe(args.python,modules,local=True)
        check('チェーン用Pythonのimport',ok,msg,
              'ANLNextのPython環境と共有ライブラリのパスを有効にしてください。EricaPyはこのPythonでビルドします。ビルド前は--mode buildを使ってください。')
    if args.mode in ['notebook','all']:
        ok,msg=probe(analysis,['ROOT','numpy','scipy','matplotlib','IPython','ipykernel','jupyter_client'])
        check('解析・ノートブック用Pythonのimport',ok,msg,
              '指定したPythonでPyROOTと表示された不足パッケージを使えるようにしてください。別環境なら--analysis-pythonを指定してください。')
        print('INFO カーネル表示名「EricaNext」の登録有無は、この確認の合否には含めません。')
        print('INFO plot_track3d.pyを使う場合は、追加でuprootとawkwardが必要です。')
    print('\n結果:', 'NG ('+str(len(failures))+'項目)' if failures else 'すべてOK')
    print('この確認は環境の予備確認です。データの有無・較正値・ビルドの成功を保証するものではありません。')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
