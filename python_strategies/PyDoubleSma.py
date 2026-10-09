from collections import deque

import crypto


class CandleAverage:
    def __init__(self, length):
        self.length = length
        self.closes = deque(maxlen=length)

    def push(self, close):
        self.closes.append(close)
        if len(self.closes) < self.length:
            return None
        return sum(self.closes) / self.length


class Strategy:
    def __init__(self, ctx, config):
        self.ctx = ctx
        self.slow = CandleAverage(int(config["slow_interval"]))
        self.fast = CandleAverage(int(config["fast_interval"]))
        self.prev_slow_above_fast = None

    def is_valid(self):
        return self.slow.length > self.fast.length

    def on_candle(self, candle):
        ts_ms = candle.close_ts_ms
        fast = self.fast.push(candle.close)
        slow = self.slow.push(candle.close)

        if fast is None or slow is None:
            return

        self.ctx.plot(ts_ms, "prices", "fast_avg_history", fast)
        self.ctx.plot(ts_ms, "prices", "slow_avg_history", slow)

        slow_above_fast = slow > fast
        crossed = self.prev_slow_above_fast is not None and slow_above_fast != self.prev_slow_above_fast
        self.prev_slow_above_fast = slow_above_fast
        if not crossed:
            return

        side = crypto.Side.sell() if slow_above_fast else crypto.Side.buy()
        self.ctx.try_send_order(side, candle.close, ts_ms)
