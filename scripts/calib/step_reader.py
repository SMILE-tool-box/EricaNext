"""Minimal STEP (AP214) assembly reader, no CAD library: world placement and vertex points of every part
instance (walks NEXT_ASSEMBLY_USAGE_OCCURRENCE / CONTEXT_DEPENDENT_SHAPE_REPRESENTATION, collects the
VERTEX_POINTs of the B-rep solids). Used by make_psa_geometry.py; run alone it prints the parts and their bounding boxes."""
import re
import sys
import numpy as np


def parse(path):
    s = open(path, errors="replace").read()
    s = s[s.index("DATA;") + 5:]
    ents = {}
    for m in re.finditer(r"#(\d+)\s*=\s*(.*?);\s*(?=#\d+\s*=|ENDSEC)", s, re.S):
        body = m.group(2).strip()
        if body.startswith("("):
            types = re.findall(r"([A-Z_0-9]+)\s*\(", body)
            ents[int(m.group(1))] = (types, body)
        else:
            t = re.match(r"([A-Z_0-9]+)", body).group(1)
            ents[int(m.group(1))] = ([t], body[len(t):])
    return ents


def refs(body):
    return [int(x) for x in re.findall(r"#(\d+)", body)]


def floats(body):
    return [float(x) for x in re.findall(r"[-+]?\d*\.?\d+(?:[Ee][-+]?\d+)?", body)]


class Step:
    def __init__(self, path):
        self.e = parse(path)
        self.by_type = {}
        for i, (ts, b) in self.e.items():
            for t in ts:
                self.by_type.setdefault(t, []).append(i)

    def point(self, i):
        b = self.e[i][1]
        return np.array(floats(b.split("(", 2)[-1])[-3:])

    def direction(self, i):
        return self.point(i)

    def axis(self, i):
        """AXIS2_PLACEMENT_3D -> 4x4 matrix (local -> parent)"""
        r = refs(self.e[i][1])
        o = self.point(r[0])
        z = self.direction(r[1]) if len(r) > 1 else np.array([0, 0, 1.])
        x = self.direction(r[2]) if len(r) > 2 else np.array([1, 0, 0.])
        z = z / np.linalg.norm(z)
        x = x - z * (x @ z); x = x / np.linalg.norm(x)
        y = np.cross(z, x)
        m = np.eye(4); m[:3, 0], m[:3, 1], m[:3, 2], m[:3, 3] = x, y, z, o
        return m

    def name(self, i):
        m = re.search(r"'([^']*)'", self.e[i][1])
        return m.group(1) if m else ""

    def build(self):
        e = self.e
        # product definition -> product name
        pdf_prod = {i: refs(e[i][1])[0] for i in self.by_type.get("PRODUCT_DEFINITION_FORMATION", []) +
                    self.by_type.get("PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE", [])}
        self.pd_name = {}
        for i in self.by_type["PRODUCT_DEFINITION"]:
            f = refs(e[i][1])[0]
            self.pd_name[i] = self.name(pdf_prod[f])
        # PDS -> definition
        pds_def = {i: refs(e[i][1])[0] for i in self.by_type["PRODUCT_DEFINITION_SHAPE"]}
        # shape representation of each product definition
        self.pd_rep = {}
        for i in self.by_type["SHAPE_DEFINITION_REPRESENTATION"]:
            p, rep = refs(e[i][1])[:2]
            d = pds_def.get(p)
            if d in self.pd_name:
                self.pd_rep[d] = rep
        # representation -> solids (via SHAPE_REPRESENTATION_RELATIONSHIP without transformation)
        self.rep_rel = {}
        for i, (ts, b) in e.items():
            if "SHAPE_REPRESENTATION_RELATIONSHIP" in ts and "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION" not in ts:
                r = refs(b)
                if len(r) >= 2:
                    self.rep_rel.setdefault(r[0], []).append(r[1]); self.rep_rel.setdefault(r[1], []).append(r[0])
        # NAUO: children with transformation
        nauo = {i: refs(e[i][1]) for i in self.by_type["NEXT_ASSEMBLY_USAGE_OCCURRENCE"]}
        self.children = {}
        for i in self.by_type["CONTEXT_DEPENDENT_SHAPE_REPRESENTATION"]:
            rr, pds = refs(e[i][1])[:2]
            ts, b = e[rr]
            r = refs(b)
            rep1, rep2, idt = r[0], r[1], r[2]
            a1, a2 = refs(e[idt][1])[:2]
            t = self.axis(a2) @ np.linalg.inv(self.axis(a1))
            occ = pds_def[pds]
            parent, child = nauo[occ][0], nauo[occ][1]
            self.children.setdefault(parent, []).append((child, t, self.name(occ), rep1, rep2))
        allc = {c for v in self.children.values() for c, *_ in v}
        self.roots = [p for p in self.pd_name if p not in allc]

    def solid_points(self, rep, seen=None):
        """vertex points of the B-rep solids of a representation (and the reps linked to it)"""
        seen = seen if seen is not None else set()
        if rep in seen:
            return []
        seen.add(rep)
        pts = []
        stack = refs(self.e[rep][1])
        visited = set()
        while stack:
            i = stack.pop()
            if i in visited or i not in self.e:
                continue
            visited.add(i)
            ts, b = self.e[i]
            t = ts[0]
            if t == "VERTEX_POINT":
                pts.append(self.point(refs(b)[0]))
            elif t in ("MANIFOLD_SOLID_BREP", "CLOSED_SHELL", "ADVANCED_FACE", "FACE_OUTER_BOUND", "FACE_BOUND",
                       "EDGE_LOOP", "ORIENTED_EDGE", "EDGE_CURVE", "BREP_WITH_VOIDS", "ORIENTED_CLOSED_SHELL",
                       "SHELL_BASED_SURFACE_MODEL", "OPEN_SHELL"):
                stack.extend(refs(b))
        for r in self.rep_rel.get(rep, []):
            pts += self.solid_points(r, seen)
        return pts

    def walk(self, pd=None, m=np.eye(4), path=""):
        """yield (path, product name, world matrix, own solid points in world)"""
        for root in ([pd] if pd else self.roots):
            name = self.pd_name[root]
            p = f"{path}/{name}"
            rep = self.pd_rep.get(root)
            own = self.solid_points(rep) if rep else []
            w = [(m @ np.append(q, 1))[:3] for q in own]
            yield p, name, m, np.array(w)
            for child, t, occ, _, _ in self.children.get(root, []):
                yield from self.walk(child, m @ t, p)


if __name__ == "__main__":
    st = Step(sys.argv[1]); st.build()
    pat = sys.argv[2] if len(sys.argv) > 2 else ""
    for p, name, m, w in st.walk():
        if pat and not re.search(pat, p):
            continue
        if len(w):
            lo, hi = w.min(0), w.max(0)
            print(f"{p}\n   bbox x {lo[0]:9.2f} {hi[0]:9.2f}  y {lo[1]:9.2f} {hi[1]:9.2f}  z {lo[2]:9.2f} {hi[2]:9.2f}  n={len(w)}")
        else:
            print(f"{p}  origin {np.round(m[:3,3],2)}")
