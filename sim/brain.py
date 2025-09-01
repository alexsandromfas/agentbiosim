"""
Interfaces e implementação de cérebros (redes neurais) para agentes.
"""
import math
import random
import numpy as np
from typing import List, Protocol, runtime_checkable, Union


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
        # Mutações nos pesos
        for layer_idx in range(len(self.weights)):
            for neuron_idx in range(len(self.weights[layer_idx])):
                for weight_idx in range(len(self.weights[layer_idx][neuron_idx])):
                    if random.random() < rate:
                        self.weights[layer_idx][neuron_idx][weight_idx] += random.gauss(0, strength)
        
        # Mutações nos biases
        for layer_idx in range(len(self.biases)):
            for neuron_idx in range(len(self.biases[layer_idx])):
                if random.random() < rate:
                    self.biases[layer_idx][neuron_idx] += random.gauss(0, strength)
        
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
        
        self.sizes[layer_idx] = new_size
        
        # Ajusta weights da camada (pesos que saem da camada anterior para esta)
        weight_layer_idx = layer_idx - 1
        if weight_layer_idx >= 0:
            old_weights = self.weights[weight_layer_idx]
            new_weights = []
            
            if new_size > old_size:
                # Adicionar neurônios
                for i in range(new_size):
                    if i < old_size:
                        # Manter neurônio existente
                        new_weights.append(list(old_weights[i]))
                    else:
                        # Criar novo neurônio com pesos aleatórios
                        fan_in = len(old_weights[0]) if old_weights else 1
                        std = 0.1 / math.sqrt(fan_in)
                        new_neuron = [random.gauss(0, std) for _ in range(fan_in)]
                        new_weights.append(new_neuron)
            else:
                # Remover neurônios (manter os primeiros)
                for i in range(new_size):
                    new_weights.append(list(old_weights[i]))
            
            self.weights[weight_layer_idx] = new_weights
        
        # Ajusta biases da camada
        if layer_idx - 1 < len(self.biases):
            old_biases = self.biases[layer_idx - 1]
            if new_size > old_size:
                # Adicionar biases
                for _ in range(new_size - old_size):
                    old_biases.append(0.0)
            else:
                # Remover biases (manter os primeiros)
                self.biases[layer_idx - 1] = old_biases[:new_size]
        
        # Ajusta weights da próxima camada (pesos que entram nesta camada)
        next_weight_layer_idx = layer_idx
        if next_weight_layer_idx < len(self.weights):
            old_next_weights = self.weights[next_weight_layer_idx]
            new_next_weights = []
            
            for neuron_weights in old_next_weights:
                if new_size > old_size:
                    # Adicionar conexões com pesos pequenos aleatórios
                    new_neuron_weights = list(neuron_weights)
                    for _ in range(new_size - old_size):
                        new_neuron_weights.append(random.gauss(0, 0.1))
                    new_next_weights.append(new_neuron_weights)
                else:
                    # Remover conexões (manter as primeiras)
                    new_next_weights.append(neuron_weights[:new_size])
            
            self.weights[next_weight_layer_idx] = new_next_weights
        # Alteração estrutural implica nova versão
        self.version += 1

# ============================================================
# Multi-brain batching utilities
# ============================================================
from typing import Sequence, Tuple, Dict, Any

# Cache simples: chave = (tuple(sizes), tuple(versions)) -> (weights_stack_list, biases_stack_list)
_multi_brain_cache: Dict[Tuple[Tuple[int, ...], Tuple[int, ...]], Tuple[list, list]] = {}
# Ordem de inserção para LRU simples
_multi_brain_cache_order: list = []  # lista de keys
# Limites (podem ser ajustados via setters externos)
_MULTI_BRAIN_CACHE_MAX_ENTRIES = 32
_MULTI_BRAIN_CACHE_MAX_MB = 512  # MB totais aproximados
_DISABLE_MULTI_BRAIN_CACHE = False
_MULTI_BRAIN_CACHE_LOG = False  # logs silenciosos por padrão

def configure_multi_brain_cache(max_entries: int = None, max_mb: int = None, disable: bool = None, log: bool = None):
    global _MULTI_BRAIN_CACHE_MAX_ENTRIES, _MULTI_BRAIN_CACHE_MAX_MB, _DISABLE_MULTI_BRAIN_CACHE, _MULTI_BRAIN_CACHE_LOG
    if max_entries is not None:
        _MULTI_BRAIN_CACHE_MAX_ENTRIES = max(0, int(max_entries))
    if max_mb is not None:
        _MULTI_BRAIN_CACHE_MAX_MB = max(0, int(max_mb))
    if disable is not None:
        _DISABLE_MULTI_BRAIN_CACHE = bool(disable)
    if log is not None:
        _MULTI_BRAIN_CACHE_LOG = bool(log)

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
    versions_key = tuple(b.version for b in brains)
    cache_key = (sizes_key, versions_key)
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
    x = inputs.astype(np.float32)
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
    x = inputs.astype(np.float32)
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
        sizes_key, versions_key = key
        top.append({
            'sizes': sizes_key,
            'num_brains': len(versions_key),
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
