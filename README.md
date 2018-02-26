# Score-P Substrate "Print Metrics"
Prints the average of registered asynchronous metrics at the end of a Score-P run. 

# Installation

Have scorep-tools available in your `$PATH`,

```
cd src
make
```

Copy the library to a location in `$LD_LIBRARY_PATH`.

# Usage

- Instrument your application with Score-P (minimal instrumentation is sufficient)
- Register an asynchronous metric (e.g., power via [https://github.com/score-p/scorep_plugin_x86_energy](x86_energy)) via `SCOREP_SUBSTRATE_PRINT_METRICS`
- Run the application
- The plugin will print the summary like
    `x86_energy/PACKAGE0/P: average 39560.283380, time 10.132306 s, participating processes 2`
    
# Environment variables

- `SCOREP_SUBSTRATE_PRINT_METRICS` a list of asynchronous metrics
- `SCOREP_SUBSTRATE_PRINT_METRICS_SEP` a separator for the single items (default == `,`)


# Details
- For any absolut metric, the average is defined as the average of all samples ever read, no matter for how long they were active.
- For any accumulating metric, the average is defined as the $`\frac{\sum_{Process=0}^N value_{last}-value_{first}}{N}`$ 
# Known issues
- In first implementations of the Substrate Plugin interface, asynchronous metrics are only available, when tracing is enabled
- Asynchronous metrics will not be available, when profiling is active
- Currently does not support asynchronous metrics per thread (e.g., [https://github.com/score-p/scorep_plugin_apapi](APAPI))