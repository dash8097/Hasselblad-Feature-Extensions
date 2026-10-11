"""离线局部触发器模型。没有设备访问、下载、时钟或默认外部输入。

compile_half_step 只计算调用者明确选择的一个时钟边沿。所有触发器必须
已确认共享时钟来源；本模型不计算传播延迟、亚稳态和外部异步输入波形。
"""


def compile_half_step(logic, cells, indices, negative):
    """将已解组合逻辑编成纯 Python；先求全部下一状态，再同时写回。"""
    cache, pending = {}, set()
    lines, updates, targets = ["def step(v):"], [], []

    def emit(ref):
        if ref in indices:
            return "v[%d]" % indices[ref]
        if isinstance(ref, int):
            if ref not in (0, 1):
                raise ValueError("常量必须为单比特")
            return str(ref)
        if ref in cache:
            return cache[ref]
        if ref in pending:
            raise ValueError("未解释的组合环")
        pending.add(ref)
        cell = logic.cell(ref)
        op = cell["op"]
        if op == "constant":
            if cell["value"] not in (0, 1):
                raise ValueError("常量必须为单比特")
            out = str(cell["value"])
        elif op == "alias":
            out = emit(cell["input"])
        else:
            inputs = [emit(node) for node in cell.get("inputs", ())]
            if op == "table":
                truth = cell["truth"]
                if not isinstance(truth, int) or truth < 0 or len(inputs) > 6:
                    raise ValueError("无效 LUT")
                index = " | ".join("(%s << %d)" % (value, bit)
                                   for bit, value in enumerate(inputs)) or "0"
                expression = "(%d >> (%s)) & 1" % (truth, index)
            elif op == "xor" and len(inputs) == 2:
                expression = "(%s ^ %s)" % tuple(inputs)
            elif op == "mux" and len(inputs) == 3:
                expression = "(%s if %s else %s)" % (inputs[2], inputs[0], inputs[1])
            else:
                raise ValueError("未显式提供的状态或边界: " + repr(ref))
            out = "t%d" % len(lines)
            lines.append("    %s = %s" % (out, expression))
        pending.remove(ref)
        cache[ref] = out
        return out

    for node, cell in cells.items():
        if cell["clock_inverted"] != negative:
            continue
        if cell["latch"]:
            raise ValueError("不能用边沿模型模拟 latch")
        data, enable, reset = (emit(cell[field]) for field in ("d", "ce", "sr"))
        reset_value = cell["sr_value"]
        if reset_value not in (0, 1):
            raise ValueError("SR 值必须为单比特")
        updates.append("(%d if %s else (%s if %s else v[%d]))" %
                       (reset_value, reset, data, enable, indices[node]))
        targets.append(indices[node])
    if not updates:
        lines.append("    pass")
    else:
        lines.append("    u = (" + ", ".join(updates) + ",)")
        for index, target in enumerate(targets):
            lines.append("    v[%d] = u[%d]" % (target, index))
    source = "\n".join(lines) + "\n"
    namespace = {}
    # 节点名仅用作字典键；生成文本只含本程序的运算符、整数和数组索引。
    exec(compile(source, "<offline-half-step>", "exec"), namespace)
    return namespace["step"], source, targets


def freeze(value):
    return tuple(freeze(item) for item in value) if isinstance(value, list) else value


class Snapshot:
    """加载本研究导出的 JSON 对象。不会打开文件或推测缺失值。"""

    def __init__(self, document):
        if document["schema_version"] != 1:
            raise ValueError("不支持的快照格式")
        self.document = document
        self.cells = {}
        for group in ("state_cells", "boundary_cells", "combinational_cells"):
            for entry in document[group]:
                node = freeze(entry["node"])
                cell = {key: freeze(value) for key, value in entry["cell"].items()}
                if node in self.cells:
                    raise ValueError("重复节点")
                self.cells[node] = cell
        self.states = {freeze(entry["node"]): self.cells[freeze(entry["node"])]
                       for entry in document["state_cells"]}
        self.boundaries = {freeze(entry["node"]): self.cells[freeze(entry["node"])]
                           for entry in document["boundary_cells"]}
        self.indices = {node: index for index, node in
                        enumerate(list(self.states) + list(self.boundaries))}
        self.positive, _, _ = compile_half_step(self, self.states, self.indices, False)
        self.negative, _, _ = compile_half_step(self, self.states, self.indices, True)

    def cell(self, node):
        if isinstance(node, int):
            return {"op": "constant", "value": node}
        return self.cells[node]

    def initial_values(self, boundary_values):
        if set(boundary_values) != set(self.boundaries):
            raise ValueError("必须显式提供全部边界值")
        values = []
        for node in self.indices:
            value = self.states[node]["init"] if node in self.states else boundary_values[node]
            if value not in (0, 1):
                raise ValueError("状态必须为单比特")
            values.append(value)
        return values

    def sampled_cycle(self, values):
        """先正沿再负沿的采样模型，不代表真实时钟频率或连续异步仿真。"""
        self.positive(values)
        self.negative(values)


def run_explicit_case(snapshot, case):
    """每项外部赋值、请求时刻和采样点均由 case 明确列出。"""
    initial = {freeze(node): value for node, value in case["initial_boundary_values"]}
    values = snapshot.initial_values(initial)
    updates = {}
    for item in case["boundary_updates"]:
        tick = item["cycle"]
        updates.setdefault(tick, []).extend(item["values"])
    probes = [snapshot.indices[freeze(node)] for node in case["probes"]]
    events, previous = [], None
    for tick in range(case["cycles"]):
        for node, value in updates.get(tick, ()):
            node = freeze(node)
            if node not in snapshot.boundaries or value not in (0, 1):
                raise ValueError("只能更新显式单比特边界")
            values[snapshot.indices[node]] = value
        snapshot.sampled_cycle(values)
        observation = tuple(values[index] for index in probes)
        if observation != previous:
            events.append((tick, observation))
            previous = observation
    return events, values
