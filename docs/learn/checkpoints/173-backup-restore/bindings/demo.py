"""Run with --module-dir pointing to the built extension's directory."""
import argparse
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--module-dir", required=True)
args = parser.parse_args()
sys.path.insert(0, args.module_dir)
import study_py as sim

game = sim.Session(seed=77)
before = game.grid()
branch = game.clone()
branch.step(sim.DROP)
print("independent branch:", branch.state_bytes() != game.state_bytes())
game.step(sim.DROP)
print("same next input:", branch.state_bytes() == game.state_bytes())
print("past grid unchanged:", all(cell == 0 for row in before for cell in row))
saved = game.state_bytes()
try:
    game.reset(seed=77, gravity_interval=0)
except ValueError:
    print("failed reset preserves state:", game.state_bytes() == saved)
