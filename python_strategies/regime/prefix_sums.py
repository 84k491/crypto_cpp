import math


class PrefixSums:
    """O(1) sum, mean and std over any [begin, end) range of values."""

    def __init__(self, values):
        self.sums = [0.0]
        self.square_sums = [0.0]
        for v in values:
            self.sums.append(self.sums[-1] + v)
            self.square_sums.append(self.square_sums[-1] + v * v)

    def sum(self, begin, end):
        return self.sums[end] - self.sums[begin]

    def mean(self, begin, end):
        return self.sum(begin, end) / (end - begin)

    def std(self, begin, end):
        count = end - begin
        mean = self.mean(begin, end)
        variance = (self.square_sums[end] - self.square_sums[begin]) / count - mean * mean
        return math.sqrt(max(variance, 0.0))
