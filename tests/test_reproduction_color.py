"""
Teste mínimo para garantir que filhos herdam a cor do pai.
Este teste evita carregar a UI (pygame) e usa as fábricas existentes.
"""
from sim.controllers import Params
from sim.entities import create_random_bacteria


def run_test():
    params = Params()
    # cria bactéria num mundo pequeno
    b = create_random_bacteria([], params, 200, 200)
    custom_color = (11, 22, 33)
    b.color = custom_color
    child = b.reproduce(params)
    if getattr(child, 'color', None) != custom_color:
        print('FAIL: child.color != parent.color', getattr(child, 'color', None), custom_color)
        raise SystemExit(2)
    print('PASS')


if __name__ == '__main__':
    run_test()
