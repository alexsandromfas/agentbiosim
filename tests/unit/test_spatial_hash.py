from sim.spatial import SpatialHash


class Obj:
    def __init__(self, x, y, r, type_code):
        self.x = x
        self.y = y
        self.r = r
        self.type_code = type_code


def test_query_ball_into_matches_query_ball_and_reuses_output():
    grid = SpatialHash(cell_size=10, width=100, height=100)
    a = Obj(10, 10, 2, 0)
    b = Obj(14, 10, 2, 1)
    c = Obj(70, 70, 2, 2)
    for obj in (a, b, c):
        grid.insert(obj, obj.x, obj.y, obj.r)

    out = {c}
    returned = grid.query_ball_into(10, 10, 10, out)

    assert returned is out
    assert returned == grid.query_ball(10, 10, 10)
    assert a in returned
    assert b in returned
    assert c not in returned


def test_query_ball_filtered_into_filters_type_codes():
    grid = SpatialHash(cell_size=10, width=100, height=100)
    food = Obj(10, 10, 2, 0)
    bacterium = Obj(14, 10, 2, 1)
    predator = Obj(16, 10, 2, 2)
    for obj in (food, bacterium, predator):
        grid.insert(obj, obj.x, obj.y, obj.r)

    out = set()
    assert grid.query_ball_filtered_into(10, 10, 12, 0, out) == {food}
    assert grid.query_ball_filtered_into(10, 10, 12, (1, 2), out) == {bacterium, predator}
    assert out == {bacterium, predator}
