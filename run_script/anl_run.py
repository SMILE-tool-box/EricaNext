"""Run an ANLNext AnalysisChain and report failure.

anlnext.AnalysisChain.run() returns nothing, also when a module fails in PreInitialize /
Initialize (e.g. an input file cannot be opened), so a script calling it would exit with 0.
run_checked() does the same steps as AnalysisChain.run() (anlnext/chain.py) and returns
False when any of them does not end with AS_OK; run_chain.py and run_sim.py exit with 1 then.
"""
import ctypes
import datetime
import sys

import anlnext
from anlnext.chain import proposed_display_period


def run_checked(chain, num_loop):
    """chain.run(num_loop), returning True only if every step ended with AS_OK."""
    if chain.display_period is None:
        chain.display_period = proposed_display_period(num_loop)
    if num_loop == -1:
        num_loop = ctypes.c_size_t(-1).value   # maximum value of size_t

    chain.define()
    chain.load_all_parameters()

    ok = True
    if chain.anl.PreInitialize() != anlnext.status.AS_OK:
        print("run_checked: PreInitialize() failed", file=sys.stderr)
        return False
    if chain.parameter_modifier is not None:
        chain.parameter_modifier(chain)
    if chain.anl.Initialize() != anlnext.status.AS_OK:
        print("run_checked: Initialize() failed", file=sys.stderr)
        return False

    print("")
    print("<Begin Analysis> | Time: " + str(datetime.datetime.now()))
    sys.stdout.flush()
    chain.anl.set_display_period(chain.display_period)
    status = chain.anl.Analyze(num_loop, chain.console)
    print("")
    print("<End Analysis>   | Time: " + str(datetime.datetime.now()))
    sys.stdout.flush()
    if status != anlnext.status.AS_OK:
        print("run_checked: Analyze() not successful", file=sys.stderr)
        ok = False

    if chain.anl.Finalize() != anlnext.status.AS_OK:
        print("run_checked: Finalize() failed", file=sys.stderr)
        ok = False
    return ok
