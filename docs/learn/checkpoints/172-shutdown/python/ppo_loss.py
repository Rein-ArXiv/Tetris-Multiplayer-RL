"""PPO's clipped surrogate and sampled diagnostics; no hard policy-distance bound."""
from numbers import Real
import math
import torch


def clipped_policy_loss(new_logp, old_logp, advantages, clip):
    if isinstance(clip, bool) or not isinstance(clip, Real) or not math.isfinite(clip) or clip <= 0:
        raise ValueError('clip must be finite and positive')
    for name, tensor in (('new_logp', new_logp), ('old_logp', old_logp), ('advantages', advantages)):
        if not isinstance(tensor, torch.Tensor) or not tensor.is_floating_point():
            raise TypeError(name + ' must be a floating tensor')
        if tensor.ndim != 1 or tensor.numel() == 0 or tensor.shape != new_logp.shape:
            raise ValueError(name + ' must share the nonempty sample axis')
        if tensor.dtype != new_logp.dtype or tensor.device != new_logp.device:
            raise ValueError(name + ' must share dtype/device')
        if not torch.isfinite(tensor).all():
            raise ValueError(name + ' must be finite')
    # Rollout targets and behavior-policy probabilities remain fixed across epochs.
    log_ratio = new_logp - old_logp.detach()
    ratio = log_ratio.exp()
    if not torch.isfinite(log_ratio).all() or not torch.isfinite(ratio).all():
        raise ValueError('policy ratio overflowed')
    advantage = advantages.detach()
    ordinary = -advantage * ratio
    clipped = -advantage * ratio.clamp(1 - clip, 1 + clip)
    loss = torch.maximum(ordinary, clipped).mean()
    with torch.no_grad():
        approximate_kl = ((ratio - 1) - log_ratio).mean()
        clip_fraction = ((ratio - 1).abs() > clip).float().mean()
    if not torch.isfinite(loss) or not torch.isfinite(approximate_kl):
        raise ValueError('PPO objective overflowed')
    return loss, approximate_kl, clip_fraction
