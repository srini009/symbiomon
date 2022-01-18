SYMBIOMON is a prototype distributed, metric monitoring service designed
for use on HPC systems. Internally, SYMBIOMON employs a time-series
data model and is composed of three microservice components:
1. COLLECTOR: Exposes the main metric API.
2. AGGREGATOR: Distributed microservice component that stores aggregated (reduced) time-series data.
3. REDUCER: Distributed microservice component that performs a global reduction on partially aggregated time-series data.
   Additionally, it makes this globally reducted value available for use in adapting distributed components.

An illustration of the SYMBIOMON design is presented below: 
![SYMBIOMON_Conceptual_Illustration(7)](https://user-images.githubusercontent.com/10570459/144708286-da263116-a128-47d6-9102-800bcd2838c4.png)

Further detailed information can be found here: https://docs.google.com/presentation/d/1SNAM1QaeQYSoRwDJfTa7bCO2IvcppCzPPFDCAUdALlU/edit?usp=sharing
