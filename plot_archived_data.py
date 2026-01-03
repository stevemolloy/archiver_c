from concurrent.futures import ProcessPoolExecutor
from datetime import datetime as dt
from matplotlib import pyplot as plt
import numpy as np
import sys
import time
import warnings
import numpy.typing as npt
from typing import Tuple, List

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

    filenames: List[str] = sys.argv[1:]
    with ProcessPoolExecutor() as executor:
        results = list(executor.map(parse_file, filenames))
    processing_time: float = time.time() - start
    print(f"Time to process {len(results)} datasets = {processing_time:0.3f} seconds")

    for line_title, values, times in results:
        plt.plot(times, values, label=line_title)

    total_time: float = time.time() - start - processing_time
    print(f"Time to plot {len(results)} x {len(times)} points = {total_time:0.3f} seconds")
    plt.legend()
    plt.show()



