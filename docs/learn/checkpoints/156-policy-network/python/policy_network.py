"""A small actor-critic graph; schema dimensions are separate from learned weights."""
from dataclasses import dataclass
import torch
from torch import nn
from model_contract import positive_size, validate_policy_inputs
from actions import dimensions


@dataclass(frozen=True)
class PolicySchema:
    rows: int
    cols: int
    pieces: int
    actions: int

    def __post_init__(self):
        for name in ('rows', 'cols', 'pieces', 'actions'):
            object.__setattr__(self, name, positive_size(getattr(self, name), name))

    @classmethod
    def from_native(cls, observation, action):
        if observation['version'] != 1:
            raise ValueError('unsupported observation schema')
        columns, orientations = dimensions(action)
        ids = tuple(positive_size(x, 'piece id') for x in observation['piece_ids'])
        if len(set(ids)) != len(ids):
            raise ValueError('piece ids must be unique')
        if observation['cols'] != columns:
            raise ValueError('observation/action column mismatch')
        return cls(observation['rows'], columns, len(ids), columns * orientations)


class PolicyNet(nn.Module):
    # Example capacity for a short CPU exercise; not the production model config.
    def __init__(self, schema, conv_channels=(4, 8), hidden=32):
        super().__init__()
        if not isinstance(schema, PolicySchema):
            raise TypeError('schema must be PolicySchema')
        if not isinstance(conv_channels, (tuple, list)) or not conv_channels:
            raise ValueError('conv_channels must be a nonempty sequence')
        channels = tuple(positive_size(c, 'conv_channels') for c in conv_channels)
        hidden = positive_size(hidden, 'hidden')
        self.schema = schema
        layers = []
        input_channels = 1  # This observation schema has one locked-occupancy plane.
        for output_channels in channels:
            layers.extend((nn.Conv2d(input_channels, output_channels, 3, padding=1),
                           nn.ReLU()))
            input_channels = output_channels
        self.trunk = nn.Sequential(*layers)
        flat = channels[-1] * schema.rows * schema.cols
        self.fuse = nn.Sequential(
            nn.Linear(flat + 2 * schema.pieces, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
        )
        self.policy_head = nn.Linear(hidden, schema.actions)
        self.value_head = nn.Linear(hidden, 1)

    def forward(self, board, current, next_piece):
        # Python metadata checks are not assertions embedded in an exported graph.
        if not torch.jit.is_tracing():
            validate_policy_inputs(
                board, current, next_piece, channels=1,
                rows=self.schema.rows, cols=self.schema.cols,
                pieces=self.schema.pieces, parameter=self.trunk[0].weight,
            )
        spatial = self.trunk(board).flatten(1)
        shared = self.fuse(torch.cat((spatial, current, next_piece), dim=-1))
        return self.policy_head(shared), self.value_head(shared).squeeze(-1)
