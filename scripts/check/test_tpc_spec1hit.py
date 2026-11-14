#!/usr/bin/env python3
"""TPC 1-hit選別の回帰検証（PyROOTのあるPythonで実行）。"""
import importlib.util
from pathlib import Path
import unittest
import ROOT

path=Path(__file__).resolve().parents[1]/'calib/make_tpc_spec1hit.py'
spec=importlib.util.spec_from_file_location('make_tpc_spec1hit',path)
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
ROOT.gInterpreter.Declare(module.CPP)

class SelectionTest(unittest.TestCase):
    def test_global_channels_and_boards(self):
        # (global FADC channel -> pulse height, expected channel 1-hit, board 1-hit)
        cases=[
            ({12:60,60:60},1,1),
            ({12:60,60:60,72:60},0,0),  # cathode signal on a different panel
            ({12:60,72:60},0,0),         # sole cathode is on the wrong panel
            ({12:60,13:60,60:60},0,1),   # two anode ASICs on one board
            ({12:60,24:60,60:60},0,0),
            ({12:60,60:60,61:60},0,1),   # two cathode ASICs on one board
            ({12:60,60:60,64:60},0,0),
            ({12:60,60:60,72:20},1,1),   # equal to threshold is not above it
            ({12:60,60:60,72:21},0,0),
            ({0:60,48:60},1,1),         # first physical board/channel
            ({47:60,95:60},1,1),        # last physical board/channel
        ]
        a=ROOT.Spec1Hit(30,20.,512)
        for pulses,channel,board in cases:
            chs=ROOT.std.vector('short')();wave=ROOT.std.vector('short')()
            for ch,height in pulses.items():
                chs.push_back(ch)
                for k in range(512):wave.push_back(height if k==40 else 0)
            n,c,b=int(a.n_events),int(a.n_1hit),int(a.n_board)
            a.fill(chs,wave)
            with self.subTest(pulses=pulses):
                self.assertEqual(int(a.n_events)-n,1)
                self.assertEqual(int(a.n_1hit)-c,channel)
                self.assertEqual(int(a.n_board)-b,board)

if __name__=='__main__':unittest.main()
