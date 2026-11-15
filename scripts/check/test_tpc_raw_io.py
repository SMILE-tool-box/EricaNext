#!/usr/bin/env python3
"""TPC rawツリーの新旧保存先と曖昧な入力の検証。"""
from pathlib import Path
import sys,tempfile,unittest
from array import array
import ROOT
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'analysis'))
from tpc_raw_io import raw_tree_path,run_info_path,NEW_TREE,LEGACY_TREE

class RawPathsTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='ericanext-raw-paths-')
    def tearDown(self):self.tmp.cleanup()
    def fixture(self,name,directories):
        path=Path(self.tmp.name)/name
        f=ROOT.TFile(str(path),'RECREATE')
        for directory in directories:
            f.mkdir(directory).cd()
            t=ROOT.TTree('raw','');event=array('i',[1]);t.Branch('event',event,'event/I');t.Fill();t.Write()
            info=ROOT.TTree('run_info','');info.Fill();info.Write();f.cd()
        f.Close();return path
    def test_new_and_old(self):
        for directory in ['TPC_RawTreeWriter','RawTreeWriter_sm3']:
            with self.subTest(directory=directory):
                p=self.fixture(directory+'.root',[directory])
                self.assertEqual(raw_tree_path(p),directory+'/raw')
                self.assertEqual(raw_tree_path(p,NEW_TREE),directory+'/raw')
                self.assertEqual(run_info_path(p),directory+'/run_info')
    def test_ambiguous(self):
        p=self.fixture('both.root',['TPC_RawTreeWriter','RawTreeWriter_sm3'])
        with self.assertRaises(ValueError):raw_tree_path(p)
        self.assertEqual(raw_tree_path(p,LEGACY_TREE),LEGACY_TREE)
    def test_explicit_smile2p(self):
        p=self.fixture('smile2p.root',['RawTreeWriter_2p'])
        self.assertEqual(raw_tree_path(p,'RawTreeWriter_2p/raw'),'RawTreeWriter_2p/raw')
        with self.assertRaises(ValueError):raw_tree_path(p)
    def test_missing_tree(self):
        p=self.fixture('empty.root',[])
        with self.assertRaises(ValueError):raw_tree_path(p)
if __name__=='__main__':unittest.main()
