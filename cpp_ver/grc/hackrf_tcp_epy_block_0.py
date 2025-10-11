import numpy as np
from gnuradio import gr

class blk(gr.sync_block):
    """Print float samples to console"""

    def __init__(self, print_every=1000):
        """
        Parameters:
        print_every -- how often to print (avoid flooding console)
        """
        gr.sync_block.__init__(
            self,
            name='Float Printer',
            in_sig=[np.int8],
            out_sig=None  # 不输出
        )
        self.print_every = int(print_every)
        self.count = 0

    def work(self, input_items, output_items):
        samples = input_items[0]
        for s in samples:
            if self.count % self.print_every == 0:
                print(f"{s:.6f}")
            self.count += 1
        return len(samples)
