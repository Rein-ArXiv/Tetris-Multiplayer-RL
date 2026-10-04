"""Short CPU mechanics exercise: fixed budget, partial rollout, independent evaluation."""
import argparse
import sys
import torch
from gym_env import RoundEnv
from rewards import RewardSpec
from policy_network import PolicySchema, PolicyNet
from ppo import Collector, update, evaluate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--module-dir', required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.module_dir)
    import study_py as sim
    torch.set_num_threads(1)
    torch.manual_seed(157)
    gamma, lam = 0.99, 0.95  # Example training configuration.
    model = PolicyNet(PolicySchema.from_native(sim.observation_schema(), sim.action_schema()))
    optimizer = torch.optim.Adam(model.parameters(), lr=3e-4)
    env = RoundEnv(max_steps=3, reward_spec=RewardSpec(gamma=gamma))
    collector = Collector(env, seed=1, fallback_discount=gamma)
    budget, rollout, done = 17, 8, 0  # Small diagnostic budget, not a trained skill level.
    try:
        while done < budget:
            model.eval()
            data = collector.collect(model, min(rollout, budget - done), device='cpu', lam=lam)
            metrics = update(model, optimizer, data)
            done += len(data['action'])
            print('decisions', done, 'batch', len(data['action']), 'metrics', metrics)
        records = evaluate(model, lambda: RoundEnv(reward_spec=RewardSpec(gamma=gamma, shaping_scale=0)),
                           seeds=(5, 7), limit=16, device='cpu')
        print('evaluation', records)
        print('ppo contract complete', 'completed episodes', len(collector.completed))
    finally:
        env.close()


if __name__ == '__main__':
    main()
