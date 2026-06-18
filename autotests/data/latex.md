This is an inline equation $E=mc^2$ and here is a display equation:

$$ \sum_{i=1}^n i = \frac{n(n+1)}{2} $$

Escaped dollars should be ignored: \$100 and \$200.

Dollars in code blocks should be ignored: `$variable` and fenced blocks:
```
$foo = $bar;
```

Nested dollars should not break display math:
$$\text{where $x$ is...}$$

Multiline display math should work:
$$
\begin{pmatrix}
1 & 0 \\
0 & 1
\end{pmatrix}
$$
