"""TPC rawツリーの新旧ROOT保存先を判別する。入力ファイルは変更しない。"""
from pathlib import Path
import ROOT

NEW_TREE = 'TPC_RawTreeWriter/raw'
LEGACY_TREE = 'RawTreeWriter_sm3/raw'


def raw_tree_path(path, tree=None):
    """既定は新名と旧名を自動判別。その他のtree指定はそのまま検証する。"""
    f=ROOT.TFile.Open(str(path))
    if not f or f.IsZombie(): raise ValueError(f'{path}: ROOTファイルを開けません')
    try:
        candidates=[NEW_TREE,LEGACY_TREE] if tree is None or tree==NEW_TREE else [tree]
        found=[name for name in candidates if f.Get(name) and f.Get(name).InheritsFrom('TTree')]
        if len(found)!=1:
            raise ValueError(f'{path}: rawツリーを一意に選べません。候補={found}、探索先={candidates}')
        return found[0]
    finally:
        f.Close()


def run_info_path(path, tree=None):
    """rawツリーと同じモジュールディレクトリのrun_infoパス。"""
    return raw_tree_path(path,tree).rsplit('/',1)[0]+'/run_info'
