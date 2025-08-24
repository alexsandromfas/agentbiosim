"""
Interfaces e implementação de cérebros (redes neurais) para agentes.
"""
import math
import random
from typing import List, Protocol, runtime_checkable


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
        """
        Args:
            sizes: [input_size, hidden1_size, hidden2_size, ..., output_size]
            init_std: Desvio padrão para inicialização dos pesos
        """
        self.sizes = list(sizes)
        self.weights = []  # List[List[List[float]]] - [layer][neuron][weight]
        self.biases = []   # List[List[float]] - [layer][neuron]
        
        # Inicializa pesos e biases
        for i in range(1, len(self.sizes)):
            rows = self.sizes[i]      # Neurônios na camada atual
            cols = self.sizes[i-1]    # Neurônios na camada anterior
            
            # Xavier/Glorot initialization
            fan_in = cols
            std = init_std / math.sqrt(fan_in if fan_in > 0 else 1)
            
            layer_weights = []
            for _ in range(rows):
                neuron_weights = [random.gauss(0, std) for _ in range(cols)]
                layer_weights.append(neuron_weights)
            
            self.weights.append(layer_weights)
            # Biases aleatórios para quebrar simetria inicial e evitar todos agentes seguirem linha reta
            if random_biases:
                # Usa um std menor para não saturar tanh rapidamente
                bias_std = std * 0.5
                self.biases.append([random.gauss(0, bias_std) for _ in range(rows)])
            else:
                self.biases.append([0.0 for _ in range(rows)])
    
    def forward(self, inputs: List[float]) -> List[float]:
        """Passada forward pela rede."""
        x = list(inputs)
        
        for layer_idx in range(len(self.weights)):
            W = self.weights[layer_idx]
            b = self.biases[layer_idx]
            
            # Calcula saídas da camada
            y = []
            for i in range(len(W)):
                # Produto escalar + bias
                activation = b[i]
                row = W[i]
                for j in range(len(row)):
                    activation += row[j] * x[j]
                y.append(activation)
            
            # Função de ativação
            if layer_idx < len(self.weights) - 1:
                # Camadas ocultas: tanh
                x = [math.tanh(v) for v in y]
            else:
                # Camada de saída: linear (deixa para o atuador interpretar)
                x = y
        
        return x
    
    def activations(self, inputs: List[float]) -> List[List[float]]:
        """Retorna ativações de todas as camadas para debug."""
        activations_per_layer = []
        x = list(inputs)
        
        for layer_idx in range(len(self.weights)):
            W = self.weights[layer_idx]
            b = self.biases[layer_idx]
            
            # Calcula pré-ativação
            y = []
            for i in range(len(W)):
                activation = b[i]
                row = W[i]
                for j in range(len(row)):
                    activation += row[j] * x[j]
                y.append(activation)
            
            # Aplica função de ativação
            if layer_idx < len(self.weights) - 1:
                x = [math.tanh(v) for v in y]
            else:
                x = y  # Saída crua
            
            activations_per_layer.append(list(x))
        
        return activations_per_layer
    
    def copy(self) -> 'NeuralNet':
        """Cria cópia profunda da rede."""
        new_net = NeuralNet(self.sizes, init_std=0.01)
        
        # Copia pesos
        new_net.weights = []
        for layer in self.weights:
            new_layer = []
            for neuron in layer:
                new_layer.append(list(neuron))
            new_net.weights.append(new_layer)
        
        # Copia biases
        new_net.biases = []
        for layer in self.biases:
            new_net.biases.append(list(layer))
        
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
        # Para cada neurônio da primeira camada, ajustar vetor de pesos
        for neuron_idx, neuron_weights in enumerate(first_layer):
            if new_input_size > old_input:
                # Adiciona pesos aleatórios
                fan_in = new_input_size
                std = 0.1 / math.sqrt(fan_in)
                for _ in range(new_input_size - old_input):
                    neuron_weights.append(random.gauss(0, std))
            else:
                # Trunca
                del neuron_weights[new_input_size:]
    
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
