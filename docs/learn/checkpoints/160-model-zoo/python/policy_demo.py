"""Forward, masked decision and a diagnostic gradient step on actual observations."""
import argparse
import sys
import numpy as np
import torch
from observation import observe, stack_batch, to_tensors
from actions import legal_mask, masked_log_softmax, entropy
from policy_network import PolicySchema, PolicyNet


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--module-dir', required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.module_dir)
    import study_py as sim
    torch.set_num_threads(1)
    torch.manual_seed(156)  # Reproducible initialization for this exercise.
    obs_schema, action_schema = sim.observation_schema(), sim.action_schema()
    schema = PolicySchema.from_native(obs_schema, action_schema)
    sessions = [sim.Session(seed) for seed in (1, 11)]
    observations = [observe(s, obs_schema) for s in sessions]
    tensors = to_tensors(stack_batch(observations))
    batch = tuple(tensors[k] for k in ('board', 'current', 'next'))
    mask = torch.from_numpy(np.stack([
        legal_mask(s, action_schema) for s in sessions]))
    model = PolicyNet(schema)
    model.eval()
    with torch.no_grad():
        logits, value = model(*batch)
        logp = masked_log_softmax(logits, mask)
        actions = logp.argmax(dim=-1)
    event = sessions[0].apply_action(int(actions[0]))
    print('shapes', [tuple(x.shape) for x in batch], tuple(logits.shape), tuple(value.shape))
    print('parameters', sum(p.numel() for p in model.parameters()),
          'selected', actions.tolist(), 'applied_ticks', event['ticks'])

    # Synthetic targets exercise the gradient plumbing, not a fitted RL objective.
    model.train()
    optimizer = torch.optim.SGD(model.parameters(), lr=0.01)
    logits, value = model(*batch)
    logp = masked_log_softmax(logits, mask)
    chosen_logp = logp.gather(1, actions[:, None]).squeeze(-1)
    target_value = torch.ones_like(value)
    loss = -chosen_logp.mean() + (value - target_value).square().mean()
    loss -= 0.01 * entropy(logp, mask).mean()
    optimizer.zero_grad(set_to_none=True)
    loss.backward()
    if not all(p.grad is not None and torch.isfinite(p.grad).all()
               for p in model.parameters()):
        raise RuntimeError('nonfinite or disconnected gradient')
    optimizer.step()
    print('policy contract complete; diagnostic loss', float(loss.detach()))


if __name__ == '__main__':
    main()
