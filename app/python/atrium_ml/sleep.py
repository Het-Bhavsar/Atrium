"""Sleep and wake from the pod log (FEATURES.md #4). Runs in the app; an estimate, not a sleep study.

Atrium method "log-epoch logistic", per 30 s epoch, built only from fields the pod already logs
(docs/ble_gatt.md section 7: activity index in mg, pulse in bpm):
 - act: log(1 + activity index), plus its mean over +-2.5 min and its max over +-5 min (sleep is long and still);
 - hr: (pulse - the night's median) / the night's interquartile range, plus its standard deviation over +-2.5 min;
 - t: hours since the first logged epoch of the night.
A logistic regression on these 6 features gives P(asleep). Unweighted training calls almost every epoch sleep,
so `fit` also picks the P(asleep) threshold at which the training nights' predicted sleep time matches the
scored sleep time (`sleep_threshold`). Trained on labelled nights, so the coefficients are data; `fit` and
`predict` are all the app needs. A 3-class version (wake, non-REM, REM) uses the same features, trained
with equal class weights.
"""
import numpy as np

EPOCH_S = 30.0


def _rolling(x, half, fn):
    out = np.empty_like(x)
    for i in range(len(x)):
        out[i] = fn(x[max(0, i - half):i + half + 1])
    return out


def epoch_features(act_mg, hr_bpm):
    """act_mg, hr_bpm: one value per 30 s epoch (nan where missing). Returns (n, 6)."""
    act = np.log1p(np.nan_to_num(np.asarray(act_mg, float), nan=0.0))
    hr = np.asarray(hr_bpm, float)
    if np.isfinite(hr).any():
        idx = np.arange(len(hr))
        good = np.isfinite(hr)
        hr = np.interp(idx, idx[good], hr[good])
        q1, med, q3 = np.percentile(hr, [25, 50, 75])
        hrz = (hr - med) / max(q3 - q1, 1.0)
    else:
        hrz = np.zeros(len(act))
    t_h = np.arange(len(act)) * EPOCH_S / 3600.0
    return np.column_stack([act, _rolling(act, 5, np.mean), _rolling(act, 10, np.max),
                            hrz, _rolling(hrz, 5, np.std), t_h])


def fit(X, y, l2=1.0, iters=300, balanced=False):
    """Multinomial logistic regression by gradient descent; y holds class indices.
    balanced=True weights classes equally (raises wake recall, but under-counts sleep time)."""
    X = np.asarray(X, float)
    mu, sd = X.mean(0), X.std(0) + 1e-9
    Z = np.column_stack([np.ones(len(X)), (X - mu) / sd])
    k = int(np.max(y)) + 1
    Y = np.eye(k)[np.asarray(y, int)]
    w = np.zeros((Z.shape[1], k))
    cw = len(y) / (k * np.maximum(Y.sum(0), 1)) if balanced else np.ones(k)
    sw = (Y * cw).sum(1)
    lr = 0.5
    for _ in range(iters):
        P = _softmax(Z @ w)
        g = Z.T @ ((P - Y) * sw[:, None]) / len(Z) + l2 * np.r_[[0], np.ones(Z.shape[1] - 1)][:, None] * w / len(Z)
        w -= lr * g
    model = {"w": w, "mu": mu, "sd": sd}
    if k == 2:
        p = predict_proba(model, X)[:, 1]
        model["sleep_threshold"] = float(np.quantile(p, 1.0 - np.mean(y)))
    return model


def predict(model, X):
    """Class per epoch. Two classes: 1 = asleep when P(asleep) >= the fitted threshold."""
    p = predict_proba(model, X)
    if p.shape[1] == 2 and "sleep_threshold" in model:
        return (p[:, 1] >= model["sleep_threshold"]).astype(int)
    return p.argmax(1)


def _softmax(a):
    a = a - a.max(1, keepdims=True)
    e = np.exp(a)
    return e / e.sum(1, keepdims=True)


def predict_proba(model, X):
    Z = np.column_stack([np.ones(len(X)), (np.asarray(X, float) - model["mu"]) / model["sd"]])
    return _softmax(Z @ model["w"])


def kappa(y, p):
    y, p = np.asarray(y), np.asarray(p)
    k = int(max(y.max(), p.max())) + 1
    cm = np.zeros((k, k))
    for a, b in zip(y, p):
        cm[a, b] += 1
    n = cm.sum()
    po = np.trace(cm) / n
    pe = (cm.sum(0) * cm.sum(1)).sum() / n ** 2
    return float((po - pe) / (1 - pe)) if pe < 1 else 0.0
