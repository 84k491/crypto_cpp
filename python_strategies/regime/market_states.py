"""Works only inside the app: `crypto` is the embedded module."""

import crypto

from regime import labeling

TO_MARKET_STATE = {
    labeling.HIGH_VOLATILITY: crypto.MarketState.HIGH_VOLATILITY,
    labeling.UP_TREND: crypto.MarketState.UP_TREND,
    labeling.DOWN_TREND: crypto.MarketState.DOWN_TREND,
    labeling.NO_TREND: crypto.MarketState.NO_TREND,
}
