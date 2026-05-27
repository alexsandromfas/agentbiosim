"""
Interfaces e implementação de cérebros (redes neurais) para agentes.
"""
import math
import random
import numpy as np
from typing import List, Protocol, runtime_checkable, Union

_BRAIN_UID_COUNTER = 1


def _new_brain_uid() -> int:
    global _BRAIN_UID_COUNTER
    uid = _BRAIN_UID_COUNTER
    _BRAIN_UID_COUNTER += 1
    return uid


@runtime_checkable
class IBrain(Protocol):
    """Interface para cérebros de agentes."""
    
    def forward(self, inputs: List[float]) -> List[float]:
        """
        Processa inputs e retorna outputs.
        
        Args:
            inputs: Lista de valores de entrada
            
        Returns:
            Lista de valores de saída
        """
        ...
    
    def activations(self, inputs: List[float]) -> List[List[float]]:
        """
        Retorna ativações de todas as camadas para debug/visualização.
        
        Args:
            inputs: Lista de valores de entrada
            
        Returns:
            Lista de listas, uma para cada camada (post-activation para hidden, raw para output)
        """
        ...


class NeuralNet:
    """
    Rede neural feedforward simples sem dependências externas.
    Suporta mutações estruturais controladas.
    """
    def __init__(self, sizes: List[int], init_std: float = 1.0, random_biases: bool = True):
        """sizes: [input, hidden..., output]; init_std controla escala inicial."""
        self.brain_type = "mlp"
        self.display_name = "MLP padrao"
        self.cache_uid = _new_brain_uid()
        self.sizes = list(sizes)
        self.version = 0  # incrementado em mutações/alterações estruturais
        self.weights: List[np.ndarray] = []
        self.biases: List[np.ndarray] = []
        for i in range(1, len(self.sizes)):
            rows = self.sizes[i]
            cols = self.sizes[i - 1]
            fan_in = cols
            std = init_std / math.sqrt(fan_in if fan_in > 0 else 1)
            w = np.random.normal(0, std, (rows, cols)).astype(np.float32)
            if random_biases:
                b_std = std * 0.5
                b = np.random.normal(0, b_std, (rows,)).astype(np.float32)
            else:
                b = np.zeros((rows,), dtype=np.float32)
            self.weights.append(w)
            self.biases.append(b)
    
    def forward(self, inputs: Union[List[float], np.ndarray]) -> List[float]:
        """Passada forward para um único input."""
        # Garantir formato numpy dos pesos
        for i, W in enumerate(self.weights):
            if isinstance(W, list):
                self.weights[i] = np.array(W, dtype=np.float32)
        for i, b in enumerate(self.biases):
            if isinstance(b, list):
                self.biases[i] = np.array(b, dtype=np.float32)
        x = np.array(inputs, dtype=np.float32)
        for layer_idx in range(len(self.weights)):
            W = self.weights[layer_idx]
            b = self.biases[layer_idx]
            x = W @ x + b
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
        return x.tolist()

    def forward_batch(self, inputs: np.ndarray) -> np.ndarray:
        """
        Passada forward para um lote de inputs.
        inputs: shape (batch, input_size)
        returns: shape (batch, output_size)
        """
        # Converter pesos/biases se ainda em lista (ex: após copy antigo)
        for i, W in enumerate(self.weights):
            if isinstance(W, list):
                self.weights[i] = np.array(W, dtype=np.float32)
        for i, b in enumerate(self.biases):
            if isinstance(b, list):
                self.biases[i] = np.array(b, dtype=np.float32)
        x = np.array(inputs, dtype=np.float32)
        for layer_idx in range(len(self.weights)):
            W = self.weights[layer_idx]
            b = self.biases[layer_idx]
            x = x @ W.T + b
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
        return x
    
    def activations(self, inputs: Union[List[float], np.ndarray]) -> List[List[float]]:
        """Retorna ativações de todas as camadas para debug (single amostra)."""
        activations_per_layer = []
        for i, W in enumerate(self.weights):
            if isinstance(W, list):
                self.weights[i] = np.array(W, dtype=np.float32)
        for i, b in enumerate(self.biases):
            if isinstance(b, list):
                self.biases[i] = np.array(b, dtype=np.float32)
        x = np.array(inputs, dtype=np.float32)
        for layer_idx in range(len(self.weights)):
            W = self.weights[layer_idx]
            b = self.biases[layer_idx]
            x = W @ x + b
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
            activations_per_layer.append(x.tolist())
        return activations_per_layer

    def activations_batch(self, inputs: np.ndarray) -> List[np.ndarray]:
        """
        Retorna ativações de todas as camadas para um lote de inputs.
        inputs: shape (batch, input_size)
        returns: List[np.ndarray] (cada shape: (batch, layer_size))
        """
        activations_per_layer = []
        for i, W in enumerate(self.weights):
            if isinstance(W, list):
                self.weights[i] = np.array(W, dtype=np.float32)
        for i, b in enumerate(self.biases):
            if isinstance(b, list):
                self.biases[i] = np.array(b, dtype=np.float32)
        x = np.array(inputs, dtype=np.float32)
        for layer_idx in range(len(self.weights)):
            W = self.weights[layer_idx]
            b = self.biases[layer_idx]
            x = x @ W.T + b
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
            activations_per_layer.append(x.copy())
        return activations_per_layer
    
    def copy(self) -> 'NeuralNet':
        """Cria cópia profunda da rede."""
        new_net = NeuralNet(self.sizes, init_std=0.01)
        new_net.version = self.version  # herda versão; mutação posterior incrementa
        new_weights: List[np.ndarray] = []
        for layer in self.weights:
            if isinstance(layer, list):
                new_weights.append(np.array(layer, dtype=np.float32))
            else:
                new_weights.append(layer.copy())
        new_biases: List[np.ndarray] = []
        for b in self.biases:
            if isinstance(b, list):
                new_biases.append(np.array(b, dtype=np.float32))
            else:
                new_biases.append(b.copy())
        new_net.weights = new_weights
        new_net.biases = new_biases
        return new_net

    def batch_key(self) -> tuple:
        """Chave leve para agrupar cerebros compativeis em forward em lote."""
        return (getattr(self, "brain_type", "mlp"), tuple(self.sizes))

    def extra_state_dict(self) -> dict:
        """Dados extras para persistencia. MLP padrao nao possui extras."""
        return {}

    def reset_runtime_state(self):
        """Zera estado temporal quando a rede possui memoria. MLP padrao nao usa."""
        return None

    def resize_input(self, new_input_size: int):
        """Redimensiona dinamicamente o tamanho da camada de entrada.
        Ajusta os pesos da primeira camada para refletir novo número de entradas.
        Útil quando sensores (ex: retina) mudam de resolução em tempo real.
        Crescimento adiciona pesos pequenos aleatórios; redução trunca.
        """
        if new_input_size <= 0:
            return
        old_input = self.sizes[0]
        if new_input_size == old_input:
            return
        self.sizes[0] = new_input_size
        if not self.weights:
            return  # Nenhuma camada oculta/saída ainda
        first_layer = self.weights[0]
        if isinstance(first_layer, list):
            # lista de listas
            for neuron_weights in first_layer:
                if new_input_size > old_input:
                    fan_in = new_input_size
                    std = 0.1 / math.sqrt(fan_in)
                    for _ in range(new_input_size - old_input):
                        neuron_weights.append(random.gauss(0, std))
                else:
                    del neuron_weights[new_input_size:]
        else:
            # ndarray
            rows = first_layer.shape[0]
            if new_input_size > old_input:
                fan_in = new_input_size
                std = 0.1 / math.sqrt(fan_in)
                extra = np.random.normal(0, std, (rows, new_input_size - old_input)).astype(np.float32)
                first_layer = np.concatenate([first_layer, extra], axis=1)
            else:
                first_layer = first_layer[:, :new_input_size]
            self.weights[0] = first_layer
    
    def mutate(self, rate: float = 0.05, strength: float = 0.1, structural_jitter: int = 0):
        """
        Aplica mutações à rede.
        
        Args:
            rate: Probabilidade de mutação por parâmetro
            strength: Desvio padrão das mutações
            structural_jitter: 0=desligado, 1=permite mudanças estruturais leves
        """
        rate = max(0.0, min(1.0, float(rate)))
        strength = max(0.0, float(strength))

        # Mutacoes numericas vetorizadas nos pesos.
        for layer_idx, weights in enumerate(self.weights):
            weights_arr = weights if isinstance(weights, np.ndarray) else np.array(weights, dtype=np.float32)
            if rate > 0.0 and strength > 0.0:
                mask = np.random.random(weights_arr.shape) < rate
                mutation_count = int(mask.sum())
                if mutation_count:
                    noise = np.random.normal(0.0, strength, mutation_count).astype(np.float32)
                    weights_arr[mask] += noise
            self.weights[layer_idx] = weights_arr

        # Mutacoes numericas vetorizadas nos biases.
        for layer_idx, biases in enumerate(self.biases):
            biases_arr = biases if isinstance(biases, np.ndarray) else np.array(biases, dtype=np.float32)
            if rate > 0.0 and strength > 0.0:
                mask = np.random.random(biases_arr.shape) < rate
                mutation_count = int(mask.sum())
                if mutation_count:
                    noise = np.random.normal(0.0, strength, mutation_count).astype(np.float32)
                    biases_arr[mask] += noise
            self.biases[layer_idx] = biases_arr
        
        # Mutações estruturais leves (se habilitado)
        if structural_jitter > 0:
            self._apply_structural_mutations()
        # Incrementa versão sempre que mutação é chamada (simplificação; evita rastrear se algo mudou)
        self.version += 1
    
    def _apply_structural_mutations(self):
        """
        Aplica mutações estruturais leves: +/- 1-2 neurônios em uma camada oculta.
        Limitado para não sobrecarregar a simulação.
        """
        if len(self.weights) <= 1:  # Só entrada->saída, não muda
            return
        
        # Probabilidade baixa de mutação estrutural
        if random.random() > 0.05:  # 5% de chance
            return
        
        # Escolhe camada oculta aleatória para modificar
        hidden_layers = len(self.weights) - 1
        if hidden_layers <= 0:
            return
        
        layer_to_modify = random.randint(0, hidden_layers - 1)
        current_size = self.sizes[layer_to_modify + 1]
        
        # Decide mudança: -2, -1, +1, +2 neurônios
        changes = [-2, -1, 1, 2]
        delta = random.choice(changes)
        new_size = max(1, min(current_size + delta, current_size * 2))  # Limites de segurança
        
        if new_size == current_size:
            return  # Sem mudança
        
        # Aplica mudança estrutural
        self._resize_layer(layer_to_modify + 1, new_size)
    
    def _resize_layer(self, layer_idx: int, new_size: int):
        """
        Redimensiona uma camada específica.
        
        Args:
            layer_idx: Índice da camada nos sizes (1-indexed para layers ocultas)
            new_size: Novo número de neurônios
        """
        old_size = self.sizes[layer_idx]
        if new_size == old_size:
            return

        for i, W in enumerate(self.weights):
            self.weights[i] = np.asarray(W, dtype=np.float32)
        for i, b in enumerate(self.biases):
            self.biases[i] = np.asarray(b, dtype=np.float32)
        
        self.sizes[layer_idx] = new_size
        
        # Ajusta weights da camada (pesos que saem da camada anterior para esta)
        weight_layer_idx = layer_idx - 1
        if weight_layer_idx >= 0:
            old_weights = np.asarray(self.weights[weight_layer_idx], dtype=np.float32)
            fan_in = old_weights.shape[1] if old_weights.ndim == 2 and old_weights.shape[1] > 0 else 1
            new_weights = np.empty((new_size, fan_in), dtype=np.float32)
            kept = min(old_size, new_size)
            new_weights[:kept, :] = old_weights[:kept, :]
            if new_size > old_size:
                std = 0.1 / math.sqrt(fan_in)
                new_weights[old_size:, :] = np.random.normal(
                    0, std, (new_size - old_size, fan_in)
                ).astype(np.float32)
            self.weights[weight_layer_idx] = new_weights
        
        # Ajusta biases da camada
        if layer_idx - 1 < len(self.biases):
            old_biases = np.asarray(self.biases[layer_idx - 1], dtype=np.float32)
            new_biases = np.zeros((new_size,), dtype=np.float32)
            kept = min(old_size, new_size)
            new_biases[:kept] = old_biases[:kept]
            self.biases[layer_idx - 1] = new_biases
        
        # Ajusta weights da próxima camada (pesos que entram nesta camada)
        next_weight_layer_idx = layer_idx
        if next_weight_layer_idx < len(self.weights):
            old_next_weights = np.asarray(self.weights[next_weight_layer_idx], dtype=np.float32)
            out_rows = old_next_weights.shape[0]
            new_next_weights = np.empty((out_rows, new_size), dtype=np.float32)
            kept = min(old_size, new_size)
            new_next_weights[:, :kept] = old_next_weights[:, :kept]
            if new_size > old_size:
                new_next_weights[:, old_size:] = np.random.normal(
                    0, 0.1, (out_rows, new_size - old_size)
                ).astype(np.float32)
            self.weights[next_weight_layer_idx] = new_next_weights
        # Alteração estrutural implica nova versão
        self.version += 1

# ============================================================
# Brain variants
# ============================================================

BRAIN_TYPE_MLP = "mlp"
BRAIN_TYPE_GATED_MLP = "gated_mlp"
BRAIN_TYPE_SHORTCUT_MLP = "shortcut_mlp"
BRAIN_TYPE_MODULATED_MLP = "modulated_mlp"
BRAIN_TYPE_SIMPLE_RNN = "simple_rnn"
BRAIN_TYPE_NEAT_COMMON = "neat_common"
BRAIN_TYPE_NEAT_SIMPLIFIED = "neat_simplified"
BRAIN_TYPE_NEAT_RECURRENT = "neat_recurrent"

_BRAIN_TYPE_ALIASES = {
    "mlp": BRAIN_TYPE_MLP,
    "padrao": BRAIN_TYPE_MLP,
    "standard": BRAIN_TYPE_MLP,
    "gated": BRAIN_TYPE_GATED_MLP,
    "gates": BRAIN_TYPE_GATED_MLP,
    "mlp_gates": BRAIN_TYPE_GATED_MLP,
    "gated_mlp": BRAIN_TYPE_GATED_MLP,
    "shortcut": BRAIN_TYPE_SHORTCUT_MLP,
    "atalho": BRAIN_TYPE_SHORTCUT_MLP,
    "shortcut_mlp": BRAIN_TYPE_SHORTCUT_MLP,
    "modulated": BRAIN_TYPE_MODULATED_MLP,
    "modulada": BRAIN_TYPE_MODULATED_MLP,
    "modulated_mlp": BRAIN_TYPE_MODULATED_MLP,
    "rnn": BRAIN_TYPE_SIMPLE_RNN,
    "simple_rnn": BRAIN_TYPE_SIMPLE_RNN,
    "neat": BRAIN_TYPE_NEAT_COMMON,
    "neat_common": BRAIN_TYPE_NEAT_COMMON,
    "common_neat": BRAIN_TYPE_NEAT_COMMON,
    "neat_comum": BRAIN_TYPE_NEAT_COMMON,
    "proto_neat": BRAIN_TYPE_NEAT_SIMPLIFIED,
    "protozoa_neat": BRAIN_TYPE_NEAT_SIMPLIFIED,
    "neat_protozoa": BRAIN_TYPE_NEAT_SIMPLIFIED,
    "neat_simplified": BRAIN_TYPE_NEAT_SIMPLIFIED,
    "neat_simplificada": BRAIN_TYPE_NEAT_SIMPLIFIED,
    "recurrent_neat": BRAIN_TYPE_NEAT_RECURRENT,
    "neat_recurrent": BRAIN_TYPE_NEAT_RECURRENT,
    "neat_recorrente": BRAIN_TYPE_NEAT_RECURRENT,
}


def normalize_brain_type(value: object) -> str:
    key = str(value or BRAIN_TYPE_MLP).strip().lower().replace(" ", "_").replace("-", "_")
    return _BRAIN_TYPE_ALIASES.get(key, BRAIN_TYPE_MLP)


def brain_type_label(value: object) -> str:
    return {
        BRAIN_TYPE_MLP: "MLP padrao",
        BRAIN_TYPE_GATED_MLP: "MLP com gates",
        BRAIN_TYPE_SHORTCUT_MLP: "MLP com atalho",
        BRAIN_TYPE_MODULATED_MLP: "MLP modulada",
        BRAIN_TYPE_SIMPLE_RNN: "RNN simples",
        BRAIN_TYPE_NEAT_COMMON: "NEAT comum",
        BRAIN_TYPE_NEAT_SIMPLIFIED: "NEAT simplificada",
        BRAIN_TYPE_NEAT_RECURRENT: "NEAT recorrente",
    }.get(normalize_brain_type(value), "MLP padrao")


def _param_float(params, key: str, default: float) -> float:
    try:
        return float(params.get(key, default)) if params is not None else float(default)
    except Exception:
        return float(default)


def _param_bool(params, key: str, default: bool) -> bool:
    try:
        return bool(params.get(key, default)) if params is not None else bool(default)
    except Exception:
        return bool(default)


def _optional_param_float(params, key: str):
    value = _param_float(params, key, -1.0)
    return value if value >= 0.0 else None


def _ensure_arrays_for(net: NeuralNet):
    for i, W in enumerate(net.weights):
        if not isinstance(W, np.ndarray):
            net.weights[i] = np.asarray(W, dtype=np.float32)
    for i, b in enumerate(net.biases):
        if not isinstance(b, np.ndarray):
            net.biases[i] = np.asarray(b, dtype=np.float32)


def _copy_base_layers(source: NeuralNet, target: NeuralNet):
    target.version = int(getattr(source, "version", 0))
    target.weights = [np.asarray(layer, dtype=np.float32).copy() for layer in getattr(source, "weights", [])]
    target.biases = [np.asarray(layer, dtype=np.float32).copy() for layer in getattr(source, "biases", [])]


def _mutate_array_inplace(arr: np.ndarray, rate: float, strength: float) -> bool:
    rate = max(0.0, min(1.0, float(rate)))
    strength = max(0.0, float(strength))
    if rate <= 0.0 or strength <= 0.0 or arr.size <= 0:
        return False
    mask = np.random.random(arr.shape) < rate
    count = int(mask.sum())
    if count <= 0:
        return False
    arr[mask] += np.random.normal(0.0, strength, count).astype(np.float32)
    return True


class GatedNeuralNet(NeuralNet):
    """MLP com moduladores escalares por neuronio oculto."""

    def __init__(
        self,
        sizes: List[int],
        init_std: float = 1.0,
        random_biases: bool = True,
        gate_init: float = 1.0,
        gate_min: float = 0.0,
        gate_max: float = 2.0,
        gate_mutation_rate: float | None = None,
        gate_mutation_strength: float | None = None,
    ):
        super().__init__(sizes, init_std=init_std, random_biases=random_biases)
        self.brain_type = BRAIN_TYPE_GATED_MLP
        self.display_name = "MLP com gates"
        self.gate_min = float(gate_min)
        self.gate_max = max(self.gate_min, float(gate_max))
        self.gate_mutation_rate = gate_mutation_rate
        self.gate_mutation_strength = gate_mutation_strength
        self.gates: List[np.ndarray] = [
            np.full((max(1, int(size)),), float(gate_init), dtype=np.float32)
            for size in self.sizes[1:-1]
        ]
        self._clamp_gates()

    def _clamp_gates(self):
        for i, gate in enumerate(getattr(self, "gates", []) or []):
            self.gates[i] = np.clip(np.asarray(gate, dtype=np.float32), self.gate_min, self.gate_max)

    def forward(self, inputs: Union[List[float], np.ndarray]) -> List[float]:
        _ensure_arrays_for(self)
        x = np.asarray(inputs, dtype=np.float32)
        for layer_idx in range(len(self.weights)):
            x = self.weights[layer_idx] @ x + self.biases[layer_idx]
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
                if layer_idx < len(self.gates):
                    x = x * self.gates[layer_idx]
        return x.tolist()

    def activations(self, inputs: Union[List[float], np.ndarray]) -> List[List[float]]:
        _ensure_arrays_for(self)
        acts = []
        x = np.asarray(inputs, dtype=np.float32)
        for layer_idx in range(len(self.weights)):
            x = self.weights[layer_idx] @ x + self.biases[layer_idx]
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
                if layer_idx < len(self.gates):
                    x = x * self.gates[layer_idx]
            acts.append(x.tolist())
        return acts

    def copy(self) -> 'GatedNeuralNet':
        new_net = GatedNeuralNet(
            self.sizes,
            init_std=0.01,
            gate_min=self.gate_min,
            gate_max=self.gate_max,
            gate_mutation_rate=self.gate_mutation_rate,
            gate_mutation_strength=self.gate_mutation_strength,
        )
        _copy_base_layers(self, new_net)
        new_net.gates = [np.asarray(g, dtype=np.float32).copy() for g in self.gates]
        return new_net

    def mutate(self, rate: float = 0.05, strength: float = 0.1, structural_jitter: int = 0):
        super().mutate(rate=rate, strength=strength, structural_jitter=structural_jitter)
        gate_rate = rate if self.gate_mutation_rate is None else self.gate_mutation_rate
        gate_strength = strength if self.gate_mutation_strength is None else self.gate_mutation_strength
        changed = False
        for gate in self.gates:
            changed = _mutate_array_inplace(gate, gate_rate, gate_strength) or changed
        if changed:
            self._clamp_gates()
            self.version += 1

    def _resize_layer(self, layer_idx: int, new_size: int):
        old_size = self.sizes[layer_idx]
        super()._resize_layer(layer_idx, new_size)
        hidden_index = layer_idx - 1
        if 0 <= hidden_index < len(self.gates):
            old_gate = np.asarray(self.gates[hidden_index], dtype=np.float32)
            new_gate = np.full((new_size,), 1.0, dtype=np.float32)
            kept = min(old_size, new_size, old_gate.shape[0])
            if kept:
                new_gate[:kept] = old_gate[:kept]
            self.gates[hidden_index] = np.clip(new_gate, self.gate_min, self.gate_max)

    def batch_key(self) -> tuple:
        return (self.brain_type, tuple(self.sizes), round(self.gate_min, 6), round(self.gate_max, 6))

    def extra_state_dict(self) -> dict:
        return {
            "brain_gate_min": self.gate_min,
            "brain_gate_max": self.gate_max,
            "brain_gate_mutation_rate": self.gate_mutation_rate,
            "brain_gate_mutation_strength": self.gate_mutation_strength,
            "brain_gates": [g.tolist() for g in self.gates],
        }


class ShortcutNeuralNet(NeuralNet):
    """MLP com atalho direto da entrada para a saida."""

    def __init__(
        self,
        sizes: List[int],
        init_std: float = 1.0,
        random_biases: bool = True,
        shortcut_init_std: float = 0.05,
        shortcut_scale: float = 0.25,
        shortcut_mutation_rate: float | None = None,
        shortcut_mutation_strength: float | None = None,
    ):
        super().__init__(sizes, init_std=init_std, random_biases=random_biases)
        self.brain_type = BRAIN_TYPE_SHORTCUT_MLP
        self.display_name = "MLP com atalho"
        self.shortcut_scale = float(shortcut_scale)
        self.shortcut_mutation_rate = shortcut_mutation_rate
        self.shortcut_mutation_strength = shortcut_mutation_strength
        output_size = int(self.sizes[-1]) if self.sizes else 1
        input_size = int(self.sizes[0]) if self.sizes else 1
        std = max(0.0, float(shortcut_init_std)) / math.sqrt(max(1, input_size))
        self.shortcut_weights = np.random.normal(0.0, std, (output_size, input_size)).astype(np.float32)
        self.shortcut_bias = np.zeros((output_size,), dtype=np.float32)

    def _shortcut(self, input_vec: np.ndarray) -> np.ndarray:
        return self.shortcut_scale * (self.shortcut_weights @ input_vec + self.shortcut_bias)

    def forward(self, inputs: Union[List[float], np.ndarray]) -> List[float]:
        _ensure_arrays_for(self)
        input_vec = np.asarray(inputs, dtype=np.float32)
        x = input_vec
        for layer_idx in range(len(self.weights)):
            x = self.weights[layer_idx] @ x + self.biases[layer_idx]
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
        x = x + self._shortcut(input_vec)
        return x.tolist()

    def activations(self, inputs: Union[List[float], np.ndarray]) -> List[List[float]]:
        _ensure_arrays_for(self)
        input_vec = np.asarray(inputs, dtype=np.float32)
        acts = []
        x = input_vec
        for layer_idx in range(len(self.weights)):
            x = self.weights[layer_idx] @ x + self.biases[layer_idx]
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
            else:
                x = x + self._shortcut(input_vec)
            acts.append(x.tolist())
        return acts

    def copy(self) -> 'ShortcutNeuralNet':
        new_net = ShortcutNeuralNet(
            self.sizes,
            init_std=0.01,
            shortcut_init_std=0.0,
            shortcut_scale=self.shortcut_scale,
            shortcut_mutation_rate=self.shortcut_mutation_rate,
            shortcut_mutation_strength=self.shortcut_mutation_strength,
        )
        _copy_base_layers(self, new_net)
        new_net.shortcut_weights = np.asarray(self.shortcut_weights, dtype=np.float32).copy()
        new_net.shortcut_bias = np.asarray(self.shortcut_bias, dtype=np.float32).copy()
        return new_net

    def resize_input(self, new_input_size: int):
        old_input = int(self.sizes[0]) if self.sizes else 0
        super().resize_input(new_input_size)
        if new_input_size <= 0 or new_input_size == old_input:
            return
        output_size = int(self.sizes[-1]) if self.sizes else 1
        old = np.asarray(self.shortcut_weights, dtype=np.float32)
        new = np.zeros((output_size, new_input_size), dtype=np.float32)
        kept = min(old.shape[1] if old.ndim == 2 else 0, new_input_size)
        if kept:
            new[:, :kept] = old[:, :kept]
        if new_input_size > kept:
            std = 0.05 / math.sqrt(max(1, new_input_size))
            new[:, kept:] = np.random.normal(0.0, std, (output_size, new_input_size - kept)).astype(np.float32)
        self.shortcut_weights = new

    def mutate(self, rate: float = 0.05, strength: float = 0.1, structural_jitter: int = 0):
        super().mutate(rate=rate, strength=strength, structural_jitter=structural_jitter)
        s_rate = rate if self.shortcut_mutation_rate is None else self.shortcut_mutation_rate
        s_strength = strength if self.shortcut_mutation_strength is None else self.shortcut_mutation_strength
        changed = _mutate_array_inplace(self.shortcut_weights, s_rate, s_strength)
        changed = _mutate_array_inplace(self.shortcut_bias, s_rate, s_strength) or changed
        if changed:
            self.version += 1

    def batch_key(self) -> tuple:
        return (self.brain_type, tuple(self.sizes), round(self.shortcut_scale, 6))

    def extra_state_dict(self) -> dict:
        return {
            "brain_shortcut_scale": self.shortcut_scale,
            "brain_shortcut_mutation_rate": self.shortcut_mutation_rate,
            "brain_shortcut_mutation_strength": self.shortcut_mutation_strength,
            "brain_shortcut_weights": self.shortcut_weights.tolist(),
            "brain_shortcut_bias": self.shortcut_bias.tolist(),
        }


class ModulatedNeuralNet(GatedNeuralNet):
    """MLP com gates e atalho entrada->saida."""

    def __init__(
        self,
        sizes: List[int],
        init_std: float = 1.0,
        random_biases: bool = True,
        gate_init: float = 1.0,
        gate_min: float = 0.0,
        gate_max: float = 2.0,
        gate_mutation_rate: float | None = None,
        gate_mutation_strength: float | None = None,
        shortcut_init_std: float = 0.05,
        shortcut_scale: float = 0.25,
        shortcut_mutation_rate: float | None = None,
        shortcut_mutation_strength: float | None = None,
    ):
        super().__init__(
            sizes,
            init_std=init_std,
            random_biases=random_biases,
            gate_init=gate_init,
            gate_min=gate_min,
            gate_max=gate_max,
            gate_mutation_rate=gate_mutation_rate,
            gate_mutation_strength=gate_mutation_strength,
        )
        self.brain_type = BRAIN_TYPE_MODULATED_MLP
        self.display_name = "MLP modulada"
        self.shortcut_scale = float(shortcut_scale)
        self.shortcut_mutation_rate = shortcut_mutation_rate
        self.shortcut_mutation_strength = shortcut_mutation_strength
        output_size = int(self.sizes[-1]) if self.sizes else 1
        input_size = int(self.sizes[0]) if self.sizes else 1
        std = max(0.0, float(shortcut_init_std)) / math.sqrt(max(1, input_size))
        self.shortcut_weights = np.random.normal(0.0, std, (output_size, input_size)).astype(np.float32)
        self.shortcut_bias = np.zeros((output_size,), dtype=np.float32)

    def _shortcut(self, input_vec: np.ndarray) -> np.ndarray:
        return self.shortcut_scale * (self.shortcut_weights @ input_vec + self.shortcut_bias)

    def forward(self, inputs: Union[List[float], np.ndarray]) -> List[float]:
        _ensure_arrays_for(self)
        input_vec = np.asarray(inputs, dtype=np.float32)
        x = input_vec
        for layer_idx in range(len(self.weights)):
            x = self.weights[layer_idx] @ x + self.biases[layer_idx]
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
                if layer_idx < len(self.gates):
                    x = x * self.gates[layer_idx]
        x = x + self._shortcut(input_vec)
        return x.tolist()

    def activations(self, inputs: Union[List[float], np.ndarray]) -> List[List[float]]:
        _ensure_arrays_for(self)
        input_vec = np.asarray(inputs, dtype=np.float32)
        acts = []
        x = input_vec
        for layer_idx in range(len(self.weights)):
            x = self.weights[layer_idx] @ x + self.biases[layer_idx]
            if layer_idx < len(self.weights) - 1:
                x = np.tanh(x)
                if layer_idx < len(self.gates):
                    x = x * self.gates[layer_idx]
            else:
                x = x + self._shortcut(input_vec)
            acts.append(x.tolist())
        return acts

    def copy(self) -> 'ModulatedNeuralNet':
        new_net = ModulatedNeuralNet(
            self.sizes,
            init_std=0.01,
            gate_min=self.gate_min,
            gate_max=self.gate_max,
            gate_mutation_rate=self.gate_mutation_rate,
            gate_mutation_strength=self.gate_mutation_strength,
            shortcut_init_std=0.0,
            shortcut_scale=self.shortcut_scale,
            shortcut_mutation_rate=self.shortcut_mutation_rate,
            shortcut_mutation_strength=self.shortcut_mutation_strength,
        )
        _copy_base_layers(self, new_net)
        new_net.gates = [np.asarray(g, dtype=np.float32).copy() for g in self.gates]
        new_net.shortcut_weights = np.asarray(self.shortcut_weights, dtype=np.float32).copy()
        new_net.shortcut_bias = np.asarray(self.shortcut_bias, dtype=np.float32).copy()
        return new_net

    def resize_input(self, new_input_size: int):
        old_input = int(self.sizes[0]) if self.sizes else 0
        GatedNeuralNet.resize_input(self, new_input_size)
        if new_input_size <= 0 or new_input_size == old_input:
            return
        output_size = int(self.sizes[-1]) if self.sizes else 1
        old = np.asarray(self.shortcut_weights, dtype=np.float32)
        new = np.zeros((output_size, new_input_size), dtype=np.float32)
        kept = min(old.shape[1] if old.ndim == 2 else 0, new_input_size)
        if kept:
            new[:, :kept] = old[:, :kept]
        if new_input_size > kept:
            std = 0.05 / math.sqrt(max(1, new_input_size))
            new[:, kept:] = np.random.normal(0.0, std, (output_size, new_input_size - kept)).astype(np.float32)
        self.shortcut_weights = new

    def mutate(self, rate: float = 0.05, strength: float = 0.1, structural_jitter: int = 0):
        GatedNeuralNet.mutate(self, rate=rate, strength=strength, structural_jitter=structural_jitter)
        s_rate = rate if self.shortcut_mutation_rate is None else self.shortcut_mutation_rate
        s_strength = strength if self.shortcut_mutation_strength is None else self.shortcut_mutation_strength
        changed = _mutate_array_inplace(self.shortcut_weights, s_rate, s_strength)
        changed = _mutate_array_inplace(self.shortcut_bias, s_rate, s_strength) or changed
        if changed:
            self.version += 1

    def batch_key(self) -> tuple:
        return (self.brain_type, tuple(self.sizes), round(self.gate_min, 6), round(self.gate_max, 6), round(self.shortcut_scale, 6))

    def extra_state_dict(self) -> dict:
        data = GatedNeuralNet.extra_state_dict(self)
        data.update({
            "brain_shortcut_scale": self.shortcut_scale,
            "brain_shortcut_mutation_rate": self.shortcut_mutation_rate,
            "brain_shortcut_mutation_strength": self.shortcut_mutation_strength,
            "brain_shortcut_weights": self.shortcut_weights.tolist(),
            "brain_shortcut_bias": self.shortcut_bias.tolist(),
        })
        return data


class SimpleRNNBrain(NeuralNet):
    """RNN simples: a primeira camada oculta recebe estado anterior curto."""

    def __init__(
        self,
        sizes: List[int],
        init_std: float = 1.0,
        random_biases: bool = True,
        recurrent_init_std: float = 0.08,
        recurrent_scale: float = 0.35,
        memory_decay: float = 0.6,
        state_clip: float = 1.0,
        reset_state_on_copy: bool = True,
        recurrent_mutation_rate: float | None = None,
        recurrent_mutation_strength: float | None = None,
    ):
        super().__init__(sizes, init_std=init_std, random_biases=random_biases)
        self.brain_type = BRAIN_TYPE_SIMPLE_RNN
        self.display_name = "RNN simples"
        self.recurrent_scale = float(recurrent_scale)
        self.memory_decay = max(0.0, min(0.999, float(memory_decay)))
        self.state_clip = max(0.01, float(state_clip))
        self.reset_state_on_copy = bool(reset_state_on_copy)
        self.recurrent_mutation_rate = recurrent_mutation_rate
        self.recurrent_mutation_strength = recurrent_mutation_strength
        state_size = int(self.sizes[1]) if len(self.sizes) > 2 else 0
        std = max(0.0, float(recurrent_init_std)) / math.sqrt(max(1, state_size))
        self.recurrent_weights = np.random.normal(0.0, std, (state_size, state_size)).astype(np.float32) if state_size > 0 else np.zeros((0, 0), dtype=np.float32)
        self.state = np.zeros((state_size,), dtype=np.float32)

    def _first_hidden(self, input_vec: np.ndarray, update_state: bool = True) -> np.ndarray:
        rec = self.recurrent_weights @ self.state if self.state.size else 0.0
        hidden = np.tanh(self.weights[0] @ input_vec + self.biases[0] + self.recurrent_scale * rec)
        if update_state and self.state.size:
            self.state = np.clip(self.memory_decay * self.state + (1.0 - self.memory_decay) * hidden, -self.state_clip, self.state_clip).astype(np.float32)
        return hidden

    def forward(self, inputs: Union[List[float], np.ndarray]) -> List[float]:
        _ensure_arrays_for(self)
        x = np.asarray(inputs, dtype=np.float32)
        if len(self.weights) > 1:
            x = self._first_hidden(x, update_state=True)
            for layer_idx in range(1, len(self.weights)):
                x = self.weights[layer_idx] @ x + self.biases[layer_idx]
                if layer_idx < len(self.weights) - 1:
                    x = np.tanh(x)
        else:
            x = self.weights[0] @ x + self.biases[0]
        return x.tolist()

    def activations(self, inputs: Union[List[float], np.ndarray]) -> List[List[float]]:
        _ensure_arrays_for(self)
        acts = []
        x = np.asarray(inputs, dtype=np.float32)
        if len(self.weights) > 1:
            x = self._first_hidden(x, update_state=False)
            acts.append(x.tolist())
            for layer_idx in range(1, len(self.weights)):
                x = self.weights[layer_idx] @ x + self.biases[layer_idx]
                if layer_idx < len(self.weights) - 1:
                    x = np.tanh(x)
                acts.append(x.tolist())
        else:
            x = self.weights[0] @ x + self.biases[0]
            acts.append(x.tolist())
        return acts

    def copy(self) -> 'SimpleRNNBrain':
        new_net = SimpleRNNBrain(
            self.sizes,
            init_std=0.01,
            recurrent_init_std=0.0,
            recurrent_scale=self.recurrent_scale,
            memory_decay=self.memory_decay,
            state_clip=self.state_clip,
            reset_state_on_copy=self.reset_state_on_copy,
            recurrent_mutation_rate=self.recurrent_mutation_rate,
            recurrent_mutation_strength=self.recurrent_mutation_strength,
        )
        _copy_base_layers(self, new_net)
        new_net.recurrent_weights = np.asarray(self.recurrent_weights, dtype=np.float32).copy()
        new_net.state = np.zeros_like(self.state) if self.reset_state_on_copy else np.asarray(self.state, dtype=np.float32).copy()
        return new_net

    def reset_runtime_state(self):
        self.state = np.zeros_like(np.asarray(self.state, dtype=np.float32))

    def mutate(self, rate: float = 0.05, strength: float = 0.1, structural_jitter: int = 0):
        super().mutate(rate=rate, strength=strength, structural_jitter=structural_jitter)
        r_rate = rate if self.recurrent_mutation_rate is None else self.recurrent_mutation_rate
        r_strength = strength if self.recurrent_mutation_strength is None else self.recurrent_mutation_strength
        if _mutate_array_inplace(self.recurrent_weights, r_rate, r_strength):
            self.version += 1

    def _resize_layer(self, layer_idx: int, new_size: int):
        super()._resize_layer(layer_idx, new_size)
        if layer_idx == 1:
            old = np.asarray(self.recurrent_weights, dtype=np.float32)
            new = np.zeros((new_size, new_size), dtype=np.float32)
            kept = min(old.shape[0] if old.ndim == 2 else 0, new_size)
            if kept:
                new[:kept, :kept] = old[:kept, :kept]
            self.recurrent_weights = new
            self.state = np.zeros((new_size,), dtype=np.float32)

    def batch_key(self) -> tuple:
        return (self.brain_type, tuple(self.sizes), round(self.recurrent_scale, 6), round(self.memory_decay, 6), round(self.state_clip, 6))

    def extra_state_dict(self) -> dict:
        return {
            "brain_recurrent_scale": self.recurrent_scale,
            "brain_memory_decay": self.memory_decay,
            "brain_state_clip": self.state_clip,
            "brain_reset_state_on_copy": self.reset_state_on_copy,
            "brain_recurrent_mutation_rate": self.recurrent_mutation_rate,
            "brain_recurrent_mutation_strength": self.recurrent_mutation_strength,
            "brain_recurrent_weights": self.recurrent_weights.tolist(),
            "brain_state": self.state.tolist(),
        }


class NEATGraphBrain:
    """Rede de topologia variavel para experimentos NEAT.

    Esta classe e isolada do caminho vetorizado. Ela existe para permitir
    topologias diferentes por organismo sem afetar as MLP/RNN em lote.
    """

    supports_batch = False

    def __init__(
        self,
        sizes: List[int],
        init_std: float = 1.0,
        brain_type: str = BRAIN_TYPE_NEAT_COMMON,
        initial_topology: str = "minimal",
        weight_init_std: float = 0.6,
        weight_mutation_rate: float | None = None,
        weight_mutation_strength: float | None = None,
        add_connection_rate: float = 0.08,
        add_node_rate: float = 0.03,
        toggle_connection_rate: float = 0.01,
        remove_connection_rate: float = 0.0,
        reset_weight_rate: float = 0.02,
        max_hidden_nodes: int = 64,
        max_connections: int = 512,
        recurrent_connection_rate: float = 0.08,
        recurrent_memory_decay: float = 0.85,
        state_clip: float = 1.0,
        reset_state_on_copy: bool = True,
    ):
        self.brain_type = normalize_brain_type(brain_type)
        self.display_name = brain_type_label(self.brain_type)
        self.cache_uid = _new_brain_uid()
        self.sizes = [max(1, int(v)) for v in (sizes or [1, 2])]
        if len(self.sizes) < 2:
            self.sizes = [self.sizes[0], 2]
        self.version = 0
        self.weights: List[np.ndarray] = []
        self.biases: List[np.ndarray] = []
        self.initial_topology = str(initial_topology or "minimal")
        if self.initial_topology not in {"minimal", "layered"}:
            self.initial_topology = "minimal"
        self.weight_init_std = max(0.0, float(weight_init_std if weight_init_std is not None else init_std))
        self.weight_mutation_rate = weight_mutation_rate
        self.weight_mutation_strength = weight_mutation_strength
        self.add_connection_rate = max(0.0, min(1.0, float(add_connection_rate)))
        self.add_node_rate = max(0.0, min(1.0, float(add_node_rate)))
        self.toggle_connection_rate = max(0.0, min(1.0, float(toggle_connection_rate)))
        self.remove_connection_rate = max(0.0, min(1.0, float(remove_connection_rate)))
        self.reset_weight_rate = max(0.0, min(1.0, float(reset_weight_rate)))
        self.max_hidden_nodes = max(0, int(max_hidden_nodes))
        self.max_connections = max(1, int(max_connections))
        self.recurrent_connection_rate = max(0.0, min(1.0, float(recurrent_connection_rate)))
        self.recurrent_memory_decay = max(0.0, min(0.999, float(recurrent_memory_decay)))
        self.state_clip = max(0.01, float(state_clip))
        self.reset_state_on_copy = bool(reset_state_on_copy)
        self.allow_recurrent_edges = self.brain_type == BRAIN_TYPE_NEAT_RECURRENT
        self.nodes: list[dict] = []
        self.connections: list[dict] = []
        self.state: dict[int, float] = {}
        self._next_node_id = 0
        self._next_innovation = 1
        self._init_graph()

    def _random_weight(self) -> float:
        return float(np.random.normal(0.0, self.weight_init_std))

    def _activation(self, value: float, kind: str) -> float:
        if kind == "linear":
            return float(value)
        if kind == "sigmoid":
            return float(1.0 / (1.0 + math.exp(-max(-60.0, min(60.0, value)))))
        return float(math.tanh(value))

    def _new_node(self, kind: str, layer: float, activation: str = "tanh") -> dict:
        node = {
            "id": int(self._next_node_id),
            "kind": str(kind),
            "layer": float(layer),
            "activation": str(activation),
        }
        self._next_node_id += 1
        self.nodes.append(node)
        if kind != "input":
            self.state[node["id"]] = 0.0
        return node

    def _node_by_id(self, node_id: int) -> dict | None:
        for node in self.nodes:
            if int(node.get("id", -1)) == int(node_id):
                return node
        return None

    def _nodes_by_kind(self, kind: str) -> list[dict]:
        return [node for node in self.nodes if node.get("kind") == kind]

    def _input_nodes(self) -> list[dict]:
        return sorted(self._nodes_by_kind("input"), key=lambda n: int(n["id"]))

    def _output_nodes(self) -> list[dict]:
        return sorted(self._nodes_by_kind("output"), key=lambda n: int(n["id"]))

    def _hidden_nodes(self) -> list[dict]:
        return sorted(self._nodes_by_kind("hidden"), key=lambda n: (float(n["layer"]), int(n["id"])))

    def _init_graph(self):
        self.nodes.clear()
        self.connections.clear()
        self.state.clear()
        self._next_node_id = 0
        self._next_innovation = 1
        input_size = int(self.sizes[0])
        output_size = int(self.sizes[-1])
        for _ in range(input_size):
            self._new_node("input", 0.0, "linear")

        if self.initial_topology == "layered" and len(self.sizes) > 2:
            hidden_layers = self.sizes[1:-1]
            denom = max(1, len(hidden_layers) + 1)
            for layer_index, count in enumerate(hidden_layers, start=1):
                layer = float(layer_index) / float(denom)
                for _ in range(max(0, int(count))):
                    self._new_node("hidden", layer, "tanh")

        for _ in range(output_size):
            self._new_node("output", 1.0, "linear")

        if self.initial_topology == "layered" and self._hidden_nodes():
            layers: list[list[dict]] = [self._input_nodes()]
            hidden_by_layer: dict[float, list[dict]] = {}
            for node in self._hidden_nodes():
                hidden_by_layer.setdefault(float(node["layer"]), []).append(node)
            for layer in sorted(hidden_by_layer):
                layers.append(hidden_by_layer[layer])
            layers.append(self._output_nodes())
            for src_layer, dst_layer in zip(layers[:-1], layers[1:]):
                for src in src_layer:
                    for dst in dst_layer:
                        self._add_connection(src["id"], dst["id"])
        else:
            for src in self._input_nodes():
                for dst in self._output_nodes():
                    self._add_connection(src["id"], dst["id"])

    def _connection_exists(self, src: int, dst: int, recurrent: bool = False) -> bool:
        for conn in self.connections:
            if int(conn["src"]) == int(src) and int(conn["dst"]) == int(dst) and bool(conn.get("recurrent", False)) == bool(recurrent):
                return True
        return False

    def _add_connection(self, src: int, dst: int, weight: float | None = None, enabled: bool = True, recurrent: bool = False) -> bool:
        if len(self.connections) >= self.max_connections:
            return False
        src_node = self._node_by_id(src)
        dst_node = self._node_by_id(dst)
        if src_node is None or dst_node is None or dst_node.get("kind") == "input":
            return False
        recurrent = bool(recurrent and self.allow_recurrent_edges)
        if not recurrent and float(src_node["layer"]) >= float(dst_node["layer"]):
            return False
        if self._connection_exists(src, dst, recurrent):
            return False
        self.connections.append({
            "src": int(src),
            "dst": int(dst),
            "weight": float(self._random_weight() if weight is None else weight),
            "enabled": bool(enabled),
            "recurrent": recurrent,
            "innovation": int(self._next_innovation),
        })
        self._next_innovation += 1
        return True

    def forward(self, inputs: Union[List[float], np.ndarray]) -> List[float]:
        values, _layers = self._compute_values(inputs, update_state=True)
        return [float(values.get(node["id"], 0.0)) for node in self._output_nodes()]

    def activations(self, inputs: Union[List[float], np.ndarray]) -> List[List[float]]:
        _values, layers = self._compute_values(inputs, update_state=False)
        return layers

    def _compute_values(self, inputs: Union[List[float], np.ndarray], update_state: bool) -> tuple[dict[int, float], list[list[float]]]:
        input_values = np.asarray(inputs, dtype=np.float32).tolist()
        if len(input_values) != self.sizes[0]:
            self.resize_input(len(input_values))
        values: dict[int, float] = {}
        for idx, node in enumerate(self._input_nodes()):
            values[int(node["id"])] = float(input_values[idx]) if idx < len(input_values) else 0.0

        incoming: dict[int, list[dict]] = {}
        for conn in self.connections:
            if bool(conn.get("enabled", True)):
                incoming.setdefault(int(conn["dst"]), []).append(conn)

        activation_layers: list[list[float]] = []
        hidden_by_layer: dict[float, list[dict]] = {}
        for node in self._hidden_nodes():
            hidden_by_layer.setdefault(float(node["layer"]), []).append(node)

        new_state = dict(self.state)
        for layer in sorted(hidden_by_layer):
            layer_values: list[float] = []
            for node in hidden_by_layer[layer]:
                total = self._node_input_sum(node, values, incoming, recurrent_allowed=True)
                out = self._activation(total, str(node.get("activation", "tanh")))
                values[int(node["id"])] = out
                new_state[int(node["id"])] = self._blend_state(int(node["id"]), out)
                layer_values.append(out)
            activation_layers.append(layer_values)

        output_values: list[float] = []
        for node in self._output_nodes():
            total = self._node_input_sum(node, values, incoming, recurrent_allowed=True)
            out = self._activation(total, str(node.get("activation", "linear")))
            values[int(node["id"])] = out
            new_state[int(node["id"])] = self._blend_state(int(node["id"]), out)
            output_values.append(out)
        activation_layers.append(output_values)

        if update_state and self.allow_recurrent_edges:
            self.state = {
                int(k): float(max(-self.state_clip, min(self.state_clip, v)))
                for k, v in new_state.items()
            }
        return values, activation_layers

    def _node_input_sum(self, node: dict, values: dict[int, float], incoming: dict[int, list[dict]], recurrent_allowed: bool) -> float:
        total = 0.0
        for conn in incoming.get(int(node["id"]), []):
            src_id = int(conn["src"])
            if bool(conn.get("recurrent", False)) and recurrent_allowed:
                src_value = float(self.state.get(src_id, 0.0))
            else:
                src_value = float(values.get(src_id, 0.0))
            total += src_value * float(conn.get("weight", 0.0))
        return total

    def _blend_state(self, node_id: int, value: float) -> float:
        if not self.allow_recurrent_edges:
            return float(value)
        old = float(self.state.get(int(node_id), 0.0))
        return float(self.recurrent_memory_decay * old + (1.0 - self.recurrent_memory_decay) * value)

    def copy(self) -> 'NEATGraphBrain':
        new = NEATGraphBrain(
            self.sizes,
            brain_type=self.brain_type,
            initial_topology=self.initial_topology,
            weight_init_std=self.weight_init_std,
            weight_mutation_rate=self.weight_mutation_rate,
            weight_mutation_strength=self.weight_mutation_strength,
            add_connection_rate=self.add_connection_rate,
            add_node_rate=self.add_node_rate,
            toggle_connection_rate=self.toggle_connection_rate,
            remove_connection_rate=self.remove_connection_rate,
            reset_weight_rate=self.reset_weight_rate,
            max_hidden_nodes=self.max_hidden_nodes,
            max_connections=self.max_connections,
            recurrent_connection_rate=self.recurrent_connection_rate,
            recurrent_memory_decay=self.recurrent_memory_decay,
            state_clip=self.state_clip,
            reset_state_on_copy=self.reset_state_on_copy,
        )
        new.nodes = [dict(node) for node in self.nodes]
        new.connections = [dict(conn) for conn in self.connections]
        new._next_node_id = int(self._next_node_id)
        new._next_innovation = int(self._next_innovation)
        new.version = int(self.version)
        new.state = {int(k): 0.0 for k in self.state} if self.reset_state_on_copy else {int(k): float(v) for k, v in self.state.items()}
        return new

    def batch_key(self) -> tuple:
        return (self.brain_type, "individual", id(self), int(self.version))

    def reset_runtime_state(self):
        self.state = {int(node["id"]): 0.0 for node in self.nodes if node.get("kind") != "input"}

    def resize_input(self, new_input_size: int):
        new_input_size = max(1, int(new_input_size))
        old_input_size = int(self.sizes[0])
        if new_input_size == old_input_size:
            return
        input_nodes = self._input_nodes()
        if new_input_size < old_input_size:
            remove_ids = {int(node["id"]) for node in input_nodes[new_input_size:]}
            self.nodes = [node for node in self.nodes if int(node["id"]) not in remove_ids]
            self.connections = [conn for conn in self.connections if int(conn["src"]) not in remove_ids and int(conn["dst"]) not in remove_ids]
        else:
            outputs = self._output_nodes()
            for _ in range(new_input_size - old_input_size):
                node = self._new_node("input", 0.0, "linear")
                for out in outputs:
                    self._add_connection(node["id"], out["id"])
        self.sizes[0] = new_input_size
        self.version += 1

    def mutate(self, rate: float = 0.05, strength: float = 0.1, structural_jitter: int = 0):
        weight_rate = rate if self.weight_mutation_rate is None else self.weight_mutation_rate
        weight_strength = strength if self.weight_mutation_strength is None else self.weight_mutation_strength
        changed = False
        for conn in self.connections:
            if random.random() < max(0.0, min(1.0, float(weight_rate))):
                conn["weight"] = float(conn.get("weight", 0.0)) + random.gauss(0.0, max(0.0, float(weight_strength)))
                changed = True
            if random.random() < self.reset_weight_rate:
                conn["weight"] = self._random_weight()
                changed = True

        if self.brain_type == BRAIN_TYPE_NEAT_SIMPLIFIED:
            changed = self._mutate_protozoa_style(rate, strength) or changed
        else:
            if random.random() < self.add_connection_rate:
                changed = self._add_random_connection() or changed
            if random.random() < self.add_node_rate:
                changed = self._add_random_node() or changed
            if random.random() < self.toggle_connection_rate:
                changed = self._toggle_random_connection() or changed
            if random.random() < self.remove_connection_rate:
                changed = self._remove_random_connection() or changed
        if changed:
            self.version += 1

    def _mutate_protozoa_style(self, rate: float, strength: float) -> bool:
        if random.random() > max(0.0, min(1.0, float(rate))):
            return False
        enabled = [conn for conn in self.connections if bool(conn.get("enabled", True)) and not bool(conn.get("recurrent", False))]
        if not enabled:
            return self._add_random_connection()
        conn = random.choice(enabled)
        if random.random() < self.add_node_rate:
            return self._split_connection(conn)
        conn["weight"] = self._random_weight() if random.random() < 0.5 else float(conn["weight"]) + random.gauss(0.0, max(0.0, float(strength)))
        return True

    def _add_random_connection(self) -> bool:
        if len(self.connections) >= self.max_connections:
            return False
        nodes = list(self.nodes)
        if len(nodes) < 2:
            return False
        targets = [n for n in nodes if n.get("kind") != "input"]
        if not targets:
            return False
        for _ in range(80):
            dst = random.choice(targets)
            src = random.choice(nodes)
            if int(src["id"]) == int(dst["id"]):
                continue
            recurrent = False
            if self.allow_recurrent_edges and random.random() < self.recurrent_connection_rate:
                recurrent = True
            elif float(src["layer"]) >= float(dst["layer"]):
                continue
            if self._add_connection(src["id"], dst["id"], recurrent=recurrent):
                return True
        return False

    def _add_random_node(self) -> bool:
        if len(self._hidden_nodes()) >= self.max_hidden_nodes:
            return False
        candidates = [conn for conn in self.connections if bool(conn.get("enabled", True)) and not bool(conn.get("recurrent", False))]
        if not candidates:
            return False
        return self._split_connection(random.choice(candidates))

    def _split_connection(self, conn: dict) -> bool:
        src = self._node_by_id(int(conn["src"]))
        dst = self._node_by_id(int(conn["dst"]))
        if src is None or dst is None:
            return False
        if len(self._hidden_nodes()) >= self.max_hidden_nodes:
            return False
        src_layer = float(src["layer"])
        dst_layer = float(dst["layer"])
        if src_layer >= dst_layer:
            return False
        conn["enabled"] = False
        node = self._new_node("hidden", (src_layer + dst_layer) * 0.5, "tanh")
        old_weight = float(conn.get("weight", 1.0))
        ok1 = self._add_connection(int(src["id"]), int(node["id"]), weight=1.0)
        ok2 = self._add_connection(int(node["id"]), int(dst["id"]), weight=old_weight)
        return bool(ok1 or ok2)

    def _toggle_random_connection(self) -> bool:
        if not self.connections:
            return False
        conn = random.choice(self.connections)
        conn["enabled"] = not bool(conn.get("enabled", True))
        return True

    def _remove_random_connection(self) -> bool:
        if not self.connections:
            return False
        idx = random.randrange(len(self.connections))
        del self.connections[idx]
        return True

    def extra_state_dict(self) -> dict:
        return {
            "brain_neat_initial_topology": self.initial_topology,
            "brain_neat_weight_init_std": self.weight_init_std,
            "brain_neat_weight_mutation_rate": self.weight_mutation_rate,
            "brain_neat_weight_mutation_strength": self.weight_mutation_strength,
            "brain_neat_add_connection_rate": self.add_connection_rate,
            "brain_neat_add_node_rate": self.add_node_rate,
            "brain_neat_toggle_connection_rate": self.toggle_connection_rate,
            "brain_neat_remove_connection_rate": self.remove_connection_rate,
            "brain_neat_reset_weight_rate": self.reset_weight_rate,
            "brain_neat_max_hidden_nodes": self.max_hidden_nodes,
            "brain_neat_max_connections": self.max_connections,
            "brain_neat_recurrent_connection_rate": self.recurrent_connection_rate,
            "brain_neat_memory_decay": self.recurrent_memory_decay,
            "brain_neat_state_clip": self.state_clip,
            "brain_neat_reset_state_on_copy": self.reset_state_on_copy,
            "brain_neat_next_node_id": self._next_node_id,
            "brain_neat_next_innovation": self._next_innovation,
            "brain_neat_nodes": [dict(node) for node in self.nodes],
            "brain_neat_connections": [dict(conn) for conn in self.connections],
            "brain_neat_state": {str(k): float(v) for k, v in self.state.items()},
        }

    def load_extra_state(self, data: dict):
        try:
            self.initial_topology = str(data.get("brain_neat_initial_topology", self.initial_topology))
            self.weight_init_std = float(data.get("brain_neat_weight_init_std", self.weight_init_std))
            if data.get("brain_neat_weight_mutation_rate", None) not in (None, "", "None"):
                self.weight_mutation_rate = float(data.get("brain_neat_weight_mutation_rate"))
            if data.get("brain_neat_weight_mutation_strength", None) not in (None, "", "None"):
                self.weight_mutation_strength = float(data.get("brain_neat_weight_mutation_strength"))
            self.add_connection_rate = max(0.0, min(1.0, float(data.get("brain_neat_add_connection_rate", self.add_connection_rate))))
            self.add_node_rate = max(0.0, min(1.0, float(data.get("brain_neat_add_node_rate", self.add_node_rate))))
            self.toggle_connection_rate = max(0.0, min(1.0, float(data.get("brain_neat_toggle_connection_rate", self.toggle_connection_rate))))
            self.remove_connection_rate = max(0.0, min(1.0, float(data.get("brain_neat_remove_connection_rate", self.remove_connection_rate))))
            self.reset_weight_rate = max(0.0, min(1.0, float(data.get("brain_neat_reset_weight_rate", self.reset_weight_rate))))
            self.max_hidden_nodes = max(0, int(float(data.get("brain_neat_max_hidden_nodes", self.max_hidden_nodes))))
            self.max_connections = max(1, int(float(data.get("brain_neat_max_connections", self.max_connections))))
            self.recurrent_connection_rate = max(0.0, min(1.0, float(data.get("brain_neat_recurrent_connection_rate", self.recurrent_connection_rate))))
            self.recurrent_memory_decay = max(0.0, min(0.999, float(data.get("brain_neat_memory_decay", self.recurrent_memory_decay))))
            self.state_clip = max(0.01, float(data.get("brain_neat_state_clip", self.state_clip)))
            self.reset_state_on_copy = str(data.get("brain_neat_reset_state_on_copy", self.reset_state_on_copy)).lower() in {"1", "true", "yes", "sim"}
            raw_nodes = _load_jsonish(data.get("brain_neat_nodes"))
            raw_connections = _load_jsonish(data.get("brain_neat_connections"))
            if isinstance(raw_nodes, list) and isinstance(raw_connections, list):
                self.nodes = [dict(node) for node in raw_nodes if isinstance(node, dict)]
                self.connections = [dict(conn) for conn in raw_connections if isinstance(conn, dict)]
                self._next_node_id = int(float(data.get("brain_neat_next_node_id", 1 + max((int(n.get("id", 0)) for n in self.nodes), default=0))))
                self._next_innovation = int(float(data.get("brain_neat_next_innovation", 1 + max((int(c.get("innovation", 0)) for c in self.connections), default=0))))
            raw_state = _load_jsonish(data.get("brain_neat_state"))
            if isinstance(raw_state, dict):
                self.state = {int(k): float(v) for k, v in raw_state.items()}
            else:
                self.state = {int(node["id"]): 0.0 for node in self.nodes if node.get("kind") != "input"}
        except Exception:
            pass


def create_brain(sizes: List[int], params=None, init_std: float = 1.0, brain_type: object | None = None):
    sizes = [max(1, int(v)) for v in (sizes or [1, 2])]
    raw_kind = brain_type
    if raw_kind is None and params is not None and hasattr(params, "get"):
        raw_kind = params.get("neural_network_type", BRAIN_TYPE_MLP)
    kind = normalize_brain_type(raw_kind)
    if kind == BRAIN_TYPE_GATED_MLP:
        return GatedNeuralNet(
            sizes,
            init_std=init_std,
            gate_init=_param_float(params, "neural_gate_init", 1.0),
            gate_min=_param_float(params, "neural_gate_min", 0.0),
            gate_max=_param_float(params, "neural_gate_max", 2.0),
            gate_mutation_rate=_optional_param_float(params, "neural_gate_mutation_rate"),
            gate_mutation_strength=_optional_param_float(params, "neural_gate_mutation_strength"),
        )
    if kind == BRAIN_TYPE_SHORTCUT_MLP:
        return ShortcutNeuralNet(
            sizes,
            init_std=init_std,
            shortcut_init_std=_param_float(params, "neural_shortcut_init_std", 0.05),
            shortcut_scale=_param_float(params, "neural_shortcut_scale", 0.25),
            shortcut_mutation_rate=_optional_param_float(params, "neural_shortcut_mutation_rate"),
            shortcut_mutation_strength=_optional_param_float(params, "neural_shortcut_mutation_strength"),
        )
    if kind == BRAIN_TYPE_MODULATED_MLP:
        return ModulatedNeuralNet(
            sizes,
            init_std=init_std,
            gate_init=_param_float(params, "neural_gate_init", 1.0),
            gate_min=_param_float(params, "neural_gate_min", 0.0),
            gate_max=_param_float(params, "neural_gate_max", 2.0),
            gate_mutation_rate=_optional_param_float(params, "neural_gate_mutation_rate"),
            gate_mutation_strength=_optional_param_float(params, "neural_gate_mutation_strength"),
            shortcut_init_std=_param_float(params, "neural_shortcut_init_std", 0.05),
            shortcut_scale=_param_float(params, "neural_shortcut_scale", 0.25),
            shortcut_mutation_rate=_optional_param_float(params, "neural_shortcut_mutation_rate"),
            shortcut_mutation_strength=_optional_param_float(params, "neural_shortcut_mutation_strength"),
        )
    if kind == BRAIN_TYPE_SIMPLE_RNN:
        return SimpleRNNBrain(
            sizes,
            init_std=init_std,
            recurrent_init_std=_param_float(params, "neural_rnn_recurrent_init_std", 0.08),
            recurrent_scale=_param_float(params, "neural_rnn_recurrent_scale", 0.35),
            memory_decay=_param_float(params, "neural_rnn_memory_decay", 0.6),
            state_clip=_param_float(params, "neural_rnn_state_clip", 1.0),
            reset_state_on_copy=_param_bool(params, "neural_rnn_reset_state_on_copy", True),
            recurrent_mutation_rate=_optional_param_float(params, "neural_rnn_mutation_rate"),
            recurrent_mutation_strength=_optional_param_float(params, "neural_rnn_mutation_strength"),
        )
    if kind in {BRAIN_TYPE_NEAT_COMMON, BRAIN_TYPE_NEAT_SIMPLIFIED, BRAIN_TYPE_NEAT_RECURRENT}:
        prefix = {
            BRAIN_TYPE_NEAT_COMMON: "neural_neat",
            BRAIN_TYPE_NEAT_SIMPLIFIED: "neural_proto_neat",
            BRAIN_TYPE_NEAT_RECURRENT: "neural_recurrent_neat",
        }[kind]
        default_topology = "minimal"
        return NEATGraphBrain(
            sizes,
            init_std=init_std,
            brain_type=kind,
            initial_topology=str(params.get(f"{prefix}_initial_topology", default_topology)) if params is not None else default_topology,
            weight_init_std=_param_float(params, f"{prefix}_weight_init_std", 0.6),
            weight_mutation_rate=_optional_param_float(params, f"{prefix}_weight_mutation_rate"),
            weight_mutation_strength=_optional_param_float(params, f"{prefix}_weight_mutation_strength"),
            add_connection_rate=_param_float(params, f"{prefix}_add_connection_rate", 0.08),
            add_node_rate=_param_float(params, f"{prefix}_add_node_rate", 0.03),
            toggle_connection_rate=_param_float(params, f"{prefix}_toggle_connection_rate", 0.01),
            remove_connection_rate=_param_float(params, f"{prefix}_remove_connection_rate", 0.0),
            reset_weight_rate=_param_float(params, f"{prefix}_reset_weight_rate", 0.02),
            max_hidden_nodes=int(_param_float(params, f"{prefix}_max_hidden_nodes", 64)),
            max_connections=int(_param_float(params, f"{prefix}_max_connections", 512)),
            recurrent_connection_rate=_param_float(params, f"{prefix}_recurrent_connection_rate", 0.08),
            recurrent_memory_decay=_param_float(params, f"{prefix}_memory_decay", 0.85),
            state_clip=_param_float(params, f"{prefix}_state_clip", 1.0),
            reset_state_on_copy=_param_bool(params, f"{prefix}_reset_state_on_copy", True),
        )
    return NeuralNet(sizes, init_std=init_std)


def _load_jsonish(value):
    if isinstance(value, str):
        try:
            import json as _json
            return _json.loads(value)
        except Exception:
            return value
    return value


def _coerce_extra_array(value, shape=None) -> np.ndarray | None:
    value = _load_jsonish(value)
    if value is None:
        return None
    try:
        arr = np.asarray(value, dtype=np.float32)
        if shape is not None and tuple(arr.shape) != tuple(shape):
            return None
        return arr
    except Exception:
        return None


def restore_brain_extras(brain: NeuralNet, data: dict):
    kind = normalize_brain_type(getattr(brain, "brain_type", data.get("brain_type", "mlp")))
    if kind in {BRAIN_TYPE_GATED_MLP, BRAIN_TYPE_MODULATED_MLP} and hasattr(brain, "gates"):
        try:
            if data.get("brain_gate_min", None) is not None:
                brain.gate_min = float(data.get("brain_gate_min"))
            if data.get("brain_gate_max", None) is not None:
                brain.gate_max = float(data.get("brain_gate_max"))
            if data.get("brain_gate_mutation_rate", None) not in (None, "", "None"):
                brain.gate_mutation_rate = float(data.get("brain_gate_mutation_rate"))
            if data.get("brain_gate_mutation_strength", None) not in (None, "", "None"):
                brain.gate_mutation_strength = float(data.get("brain_gate_mutation_strength"))
            raw = _load_jsonish(data.get("brain_gates"))
            if isinstance(raw, (list, tuple)):
                gates = []
                for i, values in enumerate(raw):
                    if i >= len(brain.gates):
                        break
                    arr = np.asarray(values, dtype=np.float32)
                    gates.append(arr if arr.shape == brain.gates[i].shape else brain.gates[i])
                if len(gates) == len(brain.gates):
                    brain.gates = gates
            brain._clamp_gates()
        except Exception:
            pass
    if kind in {BRAIN_TYPE_SHORTCUT_MLP, BRAIN_TYPE_MODULATED_MLP} and hasattr(brain, "shortcut_weights"):
        try:
            if data.get("brain_shortcut_scale", None) is not None:
                brain.shortcut_scale = float(data.get("brain_shortcut_scale"))
            if data.get("brain_shortcut_mutation_rate", None) not in (None, "", "None"):
                brain.shortcut_mutation_rate = float(data.get("brain_shortcut_mutation_rate"))
            if data.get("brain_shortcut_mutation_strength", None) not in (None, "", "None"):
                brain.shortcut_mutation_strength = float(data.get("brain_shortcut_mutation_strength"))
            arr = _coerce_extra_array(data.get("brain_shortcut_weights"), brain.shortcut_weights.shape)
            if arr is not None:
                brain.shortcut_weights = arr
            arr = _coerce_extra_array(data.get("brain_shortcut_bias"), brain.shortcut_bias.shape)
            if arr is not None:
                brain.shortcut_bias = arr
        except Exception:
            pass
    if kind == BRAIN_TYPE_SIMPLE_RNN and hasattr(brain, "recurrent_weights"):
        try:
            if data.get("brain_recurrent_scale", None) is not None:
                brain.recurrent_scale = float(data.get("brain_recurrent_scale"))
            if data.get("brain_memory_decay", None) is not None:
                brain.memory_decay = max(0.0, min(0.999, float(data.get("brain_memory_decay"))))
            if data.get("brain_state_clip", None) is not None:
                brain.state_clip = max(0.01, float(data.get("brain_state_clip")))
            if data.get("brain_reset_state_on_copy", None) not in (None, ""):
                brain.reset_state_on_copy = str(data.get("brain_reset_state_on_copy")).lower() in {"1", "true", "yes", "sim"}
            if data.get("brain_recurrent_mutation_rate", None) not in (None, "", "None"):
                brain.recurrent_mutation_rate = float(data.get("brain_recurrent_mutation_rate"))
            if data.get("brain_recurrent_mutation_strength", None) not in (None, "", "None"):
                brain.recurrent_mutation_strength = float(data.get("brain_recurrent_mutation_strength"))
            arr = _coerce_extra_array(data.get("brain_recurrent_weights"), brain.recurrent_weights.shape)
            if arr is not None:
                brain.recurrent_weights = arr
            arr = _coerce_extra_array(data.get("brain_state"), brain.state.shape)
            if arr is not None:
                brain.state = np.clip(arr, -brain.state_clip, brain.state_clip).astype(np.float32)
        except Exception:
            pass
    if kind in {BRAIN_TYPE_NEAT_COMMON, BRAIN_TYPE_NEAT_SIMPLIFIED, BRAIN_TYPE_NEAT_RECURRENT} and hasattr(brain, "load_extra_state"):
        try:
            brain.load_extra_state(data)
        except Exception:
            pass


def brain_from_data(data: dict, params=None, init_std: float = 0.01) -> NeuralNet:
    sizes = _load_jsonish(data.get("brain_sizes") or [])
    if not isinstance(sizes, list):
        sizes = []
    brain = create_brain(list(sizes) if sizes else [1, 2], params=params, init_std=init_std, brain_type=data.get("brain_type", BRAIN_TYPE_MLP))
    try:
        bw = _load_jsonish(data.get("brain_weights", []))
        bb = _load_jsonish(data.get("brain_biases", []))
        if bw and bb and len(bw) == len(bb):
            brain.weights = [np.asarray(w, dtype=np.float32) for w in bw]
            brain.biases = [np.asarray(b, dtype=np.float32) for b in bb]
    except Exception:
        pass
    restore_brain_extras(brain, data)
    try:
        brain.version = int(data.get("brain_version", getattr(brain, "version", 0)))
    except Exception:
        pass
    return brain


def brain_to_data(brain: NeuralNet) -> dict:
    data = {
        "brain_type": normalize_brain_type(getattr(brain, "brain_type", "mlp")),
        "brain_type_label": brain_type_label(getattr(brain, "brain_type", "mlp")),
        "brain_sizes": list(getattr(brain, "sizes", []) or []),
        "brain_version": int(getattr(brain, "version", 0) or 0),
        "brain_weights": [np.asarray(w, dtype=np.float32).tolist() for w in getattr(brain, "weights", [])],
        "brain_biases": [np.asarray(b, dtype=np.float32).tolist() for b in getattr(brain, "biases", [])],
    }
    try:
        data.update(brain.extra_state_dict())
    except Exception:
        pass
    return data


# ============================================================
# Multi-brain batching utilities
# ============================================================
from typing import Sequence, Tuple, Dict, Any

# Cache simples: chave = (tuple(sizes), tuple((id(brain), version), ...)).
# A identidade dos cérebros é parte da chave para evitar reutilizar pesos de
# outro grupo que por acaso tenha a mesma arquitetura e a mesma sequência de
# versões.
_multi_brain_cache: Dict[Tuple[Tuple[int, ...], Tuple[Tuple[int, int], ...]], Tuple[list, list]] = {}
# Ordem de inserção para LRU simples
_multi_brain_cache_order: list = []  # lista de keys
# Limites (podem ser ajustados via setters externos)
_MULTI_BRAIN_CACHE_MAX_ENTRIES = 32
_MULTI_BRAIN_CACHE_MAX_MB = 512  # MB totais aproximados
_DISABLE_MULTI_BRAIN_CACHE = True
_MULTI_BRAIN_CACHE_LOG = False  # logs silenciosos por padrão
_USE_NUMBA_BRAIN_FORWARD = False
_NUMBA_BRAIN_FORWARD_MIN_BATCH = 256

def configure_multi_brain_cache(max_entries: int = None, max_mb: int = None, disable: bool = None, log: bool = None, numba_forward: bool = None, numba_min_batch: int = None):
    global _MULTI_BRAIN_CACHE_MAX_ENTRIES, _MULTI_BRAIN_CACHE_MAX_MB, _DISABLE_MULTI_BRAIN_CACHE, _MULTI_BRAIN_CACHE_LOG, _USE_NUMBA_BRAIN_FORWARD, _NUMBA_BRAIN_FORWARD_MIN_BATCH
    if max_entries is not None:
        _MULTI_BRAIN_CACHE_MAX_ENTRIES = max(0, int(max_entries))
    if max_mb is not None:
        _MULTI_BRAIN_CACHE_MAX_MB = max(0, int(max_mb))
    if disable is not None:
        _DISABLE_MULTI_BRAIN_CACHE = bool(disable)
    if log is not None:
        _MULTI_BRAIN_CACHE_LOG = bool(log)
    if numba_forward is not None:
        _USE_NUMBA_BRAIN_FORWARD = bool(numba_forward)
    if numba_min_batch is not None:
        _NUMBA_BRAIN_FORWARD_MIN_BATCH = max(1, int(numba_min_batch))

def clear_multi_brain_cache(verbose: bool = False):
    """Esvazia o cache liberando memória."""
    global _multi_brain_cache, _multi_brain_cache_order
    if verbose and _multi_brain_cache and _MULTI_BRAIN_CACHE_LOG:
        try:
            print(f"[brain] Limpando cache multi_brain: {len(_multi_brain_cache)} entries")
        except Exception:
            pass
    _multi_brain_cache.clear()
    _multi_brain_cache_order.clear()
    try:
        import gc; gc.collect()
    except Exception:
        pass

def _approx_cache_total_mb() -> float:
    total_bytes = 0
    for (weight_stacks, bias_stacks) in _multi_brain_cache.values():
        for arr in weight_stacks:
            if hasattr(arr, 'nbytes'): total_bytes += arr.nbytes
        for arr in bias_stacks:
            if hasattr(arr, 'nbytes'): total_bytes += arr.nbytes
    return total_bytes / (1024*1024)

def _prune_multi_brain_cache():
    """Remove entradas mais antigas até ficar dentro dos limites."""
    if _DISABLE_MULTI_BRAIN_CACHE:
        clear_multi_brain_cache()
        return
    changed = False
    # Limita por número de entries
    while _MULTI_BRAIN_CACHE_MAX_ENTRIES >= 0 and len(_multi_brain_cache_order) > _MULTI_BRAIN_CACHE_MAX_ENTRIES:
        oldest = _multi_brain_cache_order.pop(0)
        _multi_brain_cache.pop(oldest, None)
        changed = True
    # Limita por memória aproximada
    if _MULTI_BRAIN_CACHE_MAX_MB > 0:
        total_mb = _approx_cache_total_mb()
        if total_mb > _MULTI_BRAIN_CACHE_MAX_MB:
            # Remove até ficar abaixo de 80% do limite
            target = _MULTI_BRAIN_CACHE_MAX_MB * 0.8
            while _multi_brain_cache_order and total_mb > target:
                oldest = _multi_brain_cache_order.pop(0)
                _multi_brain_cache.pop(oldest, None)
                total_mb = _approx_cache_total_mb()
                changed = True
    if changed:
        try:
            import gc; gc.collect()
            if _MULTI_BRAIN_CACHE_LOG:
                print(f"[brain] Cache multi_brain podado. entries={len(_multi_brain_cache)} mb={_approx_cache_total_mb():.1f}")
        except Exception:
            pass

def _ensure_array_layers(brain: NeuralNet):
    """Converte listas internas em np.ndarray in-place (caso legado)."""
    for i, W in enumerate(brain.weights):
        if isinstance(W, list):
            brain.weights[i] = np.array(W, dtype=np.float32)
    for i, b in enumerate(brain.biases):
        if isinstance(b, list):
            brain.biases[i] = np.array(b, dtype=np.float32)

def _build_stacks(brains: Sequence[NeuralNet]):
    if _DISABLE_MULTI_BRAIN_CACHE:
        # Construção direta sem cache
        weight_stacks = []
        bias_stacks = []
        for layer_idx in range(len(brains[0].weights)):
            layer_weights = []
            layer_biases = []
            for b in brains:
                _ensure_array_layers(b)
                layer_weights.append(b.weights[layer_idx])
                layer_biases.append(b.biases[layer_idx])
            weight_stacks.append(np.stack(layer_weights, axis=0))
            bias_stacks.append(np.stack(layer_biases, axis=0))
        return weight_stacks, bias_stacks
    sizes_key = tuple(brains[0].sizes)
    brain_identity_key = tuple((int(getattr(b, 'cache_uid', id(b))), int(getattr(b, 'version', 0))) for b in brains)
    cache_key = (sizes_key, brain_identity_key)
    cached = _multi_brain_cache.get(cache_key)
    if cached is not None:
        # move para o final (mais recente)
        try:
            if cache_key in _multi_brain_cache_order:
                _multi_brain_cache_order.remove(cache_key)
                _multi_brain_cache_order.append(cache_key)
        except Exception:
            pass
        return cached
    # Construir pilhas novas
    weight_stacks = []
    bias_stacks = []
    for layer_idx in range(len(brains[0].weights)):
        layer_weights = []
        layer_biases = []
        for b in brains:
            _ensure_array_layers(b)
            layer_weights.append(b.weights[layer_idx])
            layer_biases.append(b.biases[layer_idx])
        weight_stacks.append(np.stack(layer_weights, axis=0))
        bias_stacks.append(np.stack(layer_biases, axis=0))
    _multi_brain_cache[cache_key] = (weight_stacks, bias_stacks)
    _multi_brain_cache_order.append(cache_key)
    _prune_multi_brain_cache()
    return weight_stacks, bias_stacks


def _forward_many_brains_numba(weight_stacks: list, bias_stacks: list, inputs: np.ndarray) -> np.ndarray | None:
    """Numba backend para lotes grandes com pesos separados por agente."""
    if not _USE_NUMBA_BRAIN_FORWARD or inputs.shape[0] < _NUMBA_BRAIN_FORWARD_MIN_BATCH:
        return None
    try:
        from .fast_kernels import brain_layer_forward_kernel, has_numba
        if not has_numba():
            return None
    except Exception:
        return None
    x = np.asarray(inputs, dtype=np.float32)
    num_layers = len(weight_stacks)
    for layer_idx in range(num_layers):
        W = np.asarray(weight_stacks[layer_idx], dtype=np.float32)
        b = np.asarray(bias_stacks[layer_idx], dtype=np.float32)
        out = np.empty((x.shape[0], W.shape[1]), dtype=np.float32)
        ok = brain_layer_forward_kernel(x, W, b, layer_idx < num_layers - 1, out)
        if not ok:
            return None
        x = out
    return x


def forward_many_brains(brains: Sequence[NeuralNet], inputs: np.ndarray) -> np.ndarray:
    """Executa forward para vários cérebros (mesma arquitetura) com seus próprios pesos.

    brains: sequência de NeuralNet (mesmo sizes)
    inputs: shape (B, input_size)
    return: shape (B, output_size)
    """
    if not brains:
        return np.empty((0, 0), dtype=np.float32)
    # Verifica arquitetura homogênea
    base_sizes = brains[0].sizes
    for b in brains[1:]:
        if b.sizes != base_sizes:
            # Fallback: processa individualmente (arquitetura divergente)
            outputs = [b.forward(inp) for b, inp in zip(brains, inputs)]
            return np.array(outputs, dtype=np.float32)
    weight_stacks, bias_stacks = _build_stacks(brains)
    numba_result = _forward_many_brains_numba(weight_stacks, bias_stacks, inputs)
    if numba_result is not None:
        return numba_result
    x = np.asarray(inputs, dtype=np.float32)
    num_layers = len(weight_stacks)
    for layer_idx in range(num_layers):
        W = weight_stacks[layer_idx]      # (B,out,in)
        b = bias_stacks[layer_idx]        # (B,out)
        # x: (B,in)
        x = np.einsum('boi,bi->bo', W, x) + b
        if layer_idx < num_layers - 1:
            x = np.tanh(x)
    return x

def activations_many_brains(brains: Sequence[NeuralNet], inputs: np.ndarray) -> list:
    """Retorna ativações por camada (lista) shape (B, layer_size) cada."""
    if not brains:
        return []
    base_sizes = brains[0].sizes
    for b in brains[1:]:
        if b.sizes != base_sizes:
            # Fallback: calcula separadamente
            per = []
            for b_, inp in zip(brains, inputs):
                acts = b_.activations(inp.tolist())
                # acts é lista de listas; converter para numpy e pad
                per.append([np.array(a, dtype=np.float32) for a in acts])
            # Transpor estrutura para camada->B
            layer_lists = []
            for layer_idx in range(len(per[0])):
                layer_lists.append(np.stack([per_b[layer_idx] for per_b in per], axis=0))
            return layer_lists
    weight_stacks, bias_stacks = _build_stacks(brains)
    x = np.asarray(inputs, dtype=np.float32)
    activations = []
    num_layers = len(weight_stacks)
    for layer_idx in range(num_layers):
        W = weight_stacks[layer_idx]
        b = bias_stacks[layer_idx]
        x = np.einsum('boi,bi->bo', W, x) + b
        if layer_idx < num_layers - 1:
            x = np.tanh(x)
        activations.append(x.copy())
    return activations


# Override compativel com variantes. Mantido abaixo do helper historico para
# preservar imports existentes sem reescrever a funcao original em arquivos com
# encoding legado.
def forward_many_brains(brains: Sequence[NeuralNet], inputs: np.ndarray) -> np.ndarray:
    """Executa forward para varios cerebros compativeis em lote."""
    if not brains:
        return np.empty((0, 0), dtype=np.float32)
    if any(not bool(getattr(brain, "supports_batch", True)) for brain in brains):
        outputs = [brain.forward(inp) for brain, inp in zip(brains, inputs)]
        return np.asarray(outputs, dtype=np.float32)
    base_sizes = brains[0].sizes
    base_kind = normalize_brain_type(getattr(brains[0], "brain_type", "mlp"))
    base_key = brains[0].batch_key() if hasattr(brains[0], "batch_key") else (base_kind, tuple(base_sizes))
    for b in brains[1:]:
        key = b.batch_key() if hasattr(b, "batch_key") else (normalize_brain_type(getattr(b, "brain_type", "mlp")), tuple(getattr(b, "sizes", [])))
        if b.sizes != base_sizes or key != base_key:
            outputs = [brain.forward(inp) for brain, inp in zip(brains, inputs)]
            return np.array(outputs, dtype=np.float32)

    weight_stacks, bias_stacks = _build_stacks(brains)
    if base_kind == BRAIN_TYPE_MLP:
        numba_result = _forward_many_brains_numba(weight_stacks, bias_stacks, inputs)
        if numba_result is not None:
            return numba_result

    x = np.asarray(inputs, dtype=np.float32)
    input_x = x
    num_layers = len(weight_stacks)
    start_layer = 0

    if base_kind == BRAIN_TYPE_SIMPLE_RNN and num_layers > 1:
        W = weight_stacks[0]
        b = bias_stacks[0]
        state_size = int(W.shape[1])
        states = np.stack([
            np.asarray(getattr(br, "state", np.zeros((state_size,), dtype=np.float32)), dtype=np.float32)
            for br in brains
        ], axis=0)
        rec_w = np.stack([
            np.asarray(getattr(br, "recurrent_weights", np.zeros((state_size, state_size), dtype=np.float32)), dtype=np.float32)
            for br in brains
        ], axis=0)
        rec = np.einsum('boi,bi->bo', rec_w, states)
        scale = float(getattr(brains[0], "recurrent_scale", 0.35))
        decay = float(getattr(brains[0], "memory_decay", 0.6))
        clip = float(getattr(brains[0], "state_clip", 1.0))
        x = np.tanh(np.einsum('boi,bi->bo', W, x) + b + scale * rec)
        new_states = np.clip(decay * states + (1.0 - decay) * x, -clip, clip).astype(np.float32)
        for br, st in zip(brains, new_states):
            br.state = st
        start_layer = 1

    for layer_idx in range(num_layers):
        if layer_idx < start_layer:
            continue
        W = weight_stacks[layer_idx]
        b = bias_stacks[layer_idx]
        x = np.einsum('boi,bi->bo', W, x) + b
        if layer_idx < num_layers - 1:
            x = np.tanh(x)
            if base_kind in {BRAIN_TYPE_GATED_MLP, BRAIN_TYPE_MODULATED_MLP}:
                try:
                    gates = np.stack([np.asarray(br.gates[layer_idx], dtype=np.float32) for br in brains], axis=0)
                    x = x * gates
                except Exception:
                    pass

    if base_kind in {BRAIN_TYPE_SHORTCUT_MLP, BRAIN_TYPE_MODULATED_MLP}:
        try:
            sw = np.stack([np.asarray(br.shortcut_weights, dtype=np.float32) for br in brains], axis=0)
            sb = np.stack([np.asarray(br.shortcut_bias, dtype=np.float32) for br in brains], axis=0)
            scale = float(getattr(brains[0], "shortcut_scale", 0.25))
            x = x + scale * (np.einsum('boi,bi->bo', sw, input_x) + sb)
        except Exception:
            outputs = [brain.forward(inp) for brain, inp in zip(brains, inputs)]
            return np.array(outputs, dtype=np.float32)
    return x

# ============================================================
# Debug / Memory inspection helpers
# ============================================================
def get_multi_brain_cache_stats(limit_detail: int = 5) -> dict:
    """Retorna estatísticas sobre o cache de batching multi-cérebro.

    limit_detail: quantos maiores entries detalhar.
    """
    import sys
    total_entries = len(_multi_brain_cache)
    entry_sizes = []  # (bytes, key)
    total_bytes = 0
    total_arrays = 0
    for key, (weight_stacks, bias_stacks) in _multi_brain_cache.items():
        entry_bytes = 0
        # weight_stacks e bias_stacks são listas de np.ndarray
        for arr in weight_stacks:
            if hasattr(arr, 'nbytes'):
                entry_bytes += arr.nbytes
                total_arrays += 1
        for arr in bias_stacks:
            if hasattr(arr, 'nbytes'):
                entry_bytes += arr.nbytes
                total_arrays += 1
        total_bytes += entry_bytes
        entry_sizes.append((entry_bytes, key))
    entry_sizes.sort(reverse=True)
    top = []
    for b, key in entry_sizes[:limit_detail]:
        sizes_key, brain_identity_key = key
        top.append({
            'sizes': sizes_key,
            'num_brains': len(brain_identity_key),
            'approx_mb': round(b / (1024*1024), 2)
        })
    return {
        'entries': total_entries,
        'total_arrays': total_arrays,
        'approx_total_mb': round(total_bytes / (1024*1024), 2),
        'largest': top
    }

def estimate_brains_param_memory(brains: Sequence[NeuralNet]) -> dict:
    """Estimativa de memória ocupada pelos parâmetros dos cérebros atuais."""
    import numpy as _np
    total_params = 0
    total_bytes = 0
    distinct_archs = set()
    versions = []
    for b in brains:
        if not hasattr(b, 'weights'):
            continue
        arch = tuple(getattr(b, 'sizes', []))
        distinct_archs.add(arch)
        versions.append(getattr(b, 'version', -1))
        for W in b.weights:
            if isinstance(W, list):
                arr = _np.array(W, dtype=_np.float32)
            else:
                arr = W
            total_params += arr.size
            total_bytes += arr.nbytes
        for B in b.biases:
            if isinstance(B, list):
                arr = _np.array(B, dtype=_np.float32)
            else:
                arr = B
            total_params += arr.size
            total_bytes += arr.nbytes
    return {
        'brains': len(brains),
        'distinct_archs': len(distinct_archs),
        'total_params': total_params,
        'approx_param_mb': round(total_bytes / (1024*1024), 2),
    }
