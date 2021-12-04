SYMBIMON is a prototype distributed, metric monitoring service designed
for use on HPC systems. Internally, SYMBIOMON employs a time-series
data model and is composed of three microservice components:
1. COLLECTOR: Exposes the main metric API.
2. AGGREGATOR: Distributed microservice component that stores aggregated (reduced) time-series data.
3. REDUCER: Distributed microservice component that performs a global redution on partially aggregated time-series data.
   Additionally, it makes this globally reducted value available for use in adapting distributed components.
