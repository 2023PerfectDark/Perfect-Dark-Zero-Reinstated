from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/coop_ally.cpp')
s=p.read_text().replace('constexpr char name[]="codex_coop_ally";', '''// Runtime clones are not registered XML assets. A named actor causes the
  // native constructor to look up and retain its nonexistent XML owner.
  // An anonymous setup takes the supported no-registration branch instead.
  constexpr char name[]="";''')
p.write_text(s)
