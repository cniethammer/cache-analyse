#!/usr/bin/env python2

import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib

matplotlib.style.use('ggplot')

filename = sys.argv[1]
print filename


data = pd.read_csv(filename, sep='\t', index_col=False)
df = pd.DataFrame()

for mnode in data['mnode'].unique() :
    sub = data[(data.mnode==mnode)]
    pyscpu = sub['pyscpu']
    #if not 'pyscpu' in df.columns :
    #    df['pyscpu'] = pyscpu
    #print pyscpu
    sub = sub.drop(['mnode', 'pyscpu'], 1)
    cycl = sub.mean(axis=1)
    cycl.index = pyscpu
    df[str(mnode)] = pd.Series(cycl)

print df
diagram = df.plot(kind='bar', title="Memory lateny per core and numa domain")
diagram.set_ylabel("latency in CPU cycles")

plt.legend(loc='center left', bbox_to_anchor=(1.0, 0.5))
plt.show()
