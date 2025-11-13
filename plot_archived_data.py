from datetime import datetime as dt
from matplotlib import pyplot as plt
import numpy as np
import sys
import time
import warnings
import numpy.typing as npt
from typing import Tuple

narray_f64 = npt.NDArray[np.float64]
narray_dt = npt.NDArray[np.datetime64]

def parse_file(filename: str) -> Tuple[str, narray_f64, narray_dt]:
    with open(filename) as f:
        header: str = f.readline()
        line_title: str = header.split("tango://g-v-csdb-0.maxiv.lu.se:10000/")[1][:-2]
        next(f)
    
        data: npt.NDArray[np.str_] = np.loadtxt(f, delimiter=",", dtype=str, ndmin=2)
    
        times_str: npt.NDArray[np.str_] = np.char.replace(data[:, 0], "_", "T")
        times: narray_dt = times_str.astype("datetime64[ns]")
        values: narray_f64 = data[:, 1].astype(float)

    return line_title, values, times

if __name__ == "__main__":
    warnings.filterwarnings(action="ignore", category=UserWarning)

    if len(sys.argv) < 2:
        raise Exception("No files provided to parse")

    start: float = time.time()
    for argc in sys.argv[1:]:
        print(time.time() - start)
        line_title, values, times = parse_file(argc)
        plt.plot(times, values, label=line_title)
    
    print(time.time() - start)
    plt.legend()
    plt.show()



