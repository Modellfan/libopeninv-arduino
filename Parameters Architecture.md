# 🧭 Design Decision: Static vs Dynamic Parameters

## **Decision Summary**

**Decision:** The standard parameter system will use **exclusively static (compile-time) parameters**.  
**Reason:** The embedded environment must be deterministic, efficient, and free of scripting or runtime parameter mutation. Dynamic parameters are only needed for external datalogging applications and will be implemented later in a **separate ParameterManager instance**, not in the embedded core.

---

## **Problem Statement**

We need a parameter architecture that:

- Minimizes RAM and CPU usage on small embedded systems (Arduino, STM32)
- Guarantees deterministic behavior without heap fragmentation
- Keeps parameter definitions simple and one-line
- Ensures static compile-time metadata stored in Flash
- Optionally supports dynamic, user-defined parameters for dataloggers — but **not** in the constrained embedded runtime

The embedded firmware must remain safe, predictable, and optimized. Therefore, configurable runtime parameters are **not** allowed inside the embedded environment.

---

## **Options Considered**

---

### **1. Static Parameters Only**

Static parameters are defined at compile time with metadata stored in Flash.

#### **Pros**
- Smallest RAM footprint  
- Fully deterministic behavior  
- No heap usage  
- Highest performance  
- Compile-time validation possible  
- Clean, single-line macro definitions  
- Very safe for embedded systems  

#### **Cons**
- Cannot add parameters at runtime  
- Requires firmware rebuild for changes  
- Metadata cannot be modified dynamically  

---

### **2. Dynamic Parameters Only**

All parameters are created from JSON/config/network at runtime.

#### **Pros**
- Fully flexible  
- Unlimited user-defined parameters  
- Metadata editable dynamically  

#### **Cons**
- Requires heap or memory pool  
- Larger RAM footprint (metadata in RAM)  
- Risk of memory fragmentation  
- Harder to optimize  
- No compile-time guarantees  
- Not suitable for embedded firmware without scripting support  

---

## **Final Decision**

**Use static parameters exclusively for the embedded firmware.**

Dynamic parameter creation is **not included** in the standard embedded ParameterManager, because:

- Embedded firmware must stay deterministic  
- No scripting/config interpretation allowed on the embedded target  
- Heap usage and runtime metadata are undesirable  

However:

### **Dynamic parameter support will be added later**  
→ **in a separate, interface-compatible ParameterManager implementation**  
→ **intended for datalogging, tools, or desktop applications**  
→ **never used inside the constrained embedded firmware**  

This ensures:

- Embedded system = safe, static, minimal  
- Tools/dataloggers = flexible, dynamic  

Both can share the same high-level API, but **not the same runtime behavior**.

---

# 🧭 Design Decision: Parameter Self-Registration

## **Decision Summary**

**Decision:** Use **self-registration** for all parameters during construction.  
**Reason:** This enables high modularity and allows each software component to define its own parameters without a central list. A fully static, centralized parameter list would require extra tooling (e.g. Python pre-build scripts) and is harder to maintain and debug.

---

## **Problem Statement**

We want parameters to be:

- Split across multiple files, located where the signal or feature is implemented  
- Automatically collected by the `ParameterManager`  
- Avoid manual bookkeeping or centralized files  
- Avoid complex pre-build steps or external scripts  
- Work for both static and (optional future) dynamic parameters  

---

## **Options Considered**

### **1. Fully Static Registry in Flash (No Self-Registration)**

The `ParameterManager` holds a `static const` list of parameter pointers in flash:

```cpp
extern StaticParameter<float> engineTemp;
extern StaticParameter<int>   rpm;

static const IParameter* ALL_PARAMS[] = {
    &engineTemp,
    &rpm,
    // ...
};
```

#### **Pros**
- Registry list can live entirely in **Flash** (no RAM used for registration)  
- No constructor side effects  
- Very predictable and optimal for purely static systems  

#### **Cons**
- Requires a **central list** of all parameters  
- Poor modularity: parameters defined in multiple modules must still be added to one central array  
- A truly distributed static registry in pure C++ requires **precompile/codegen scripts** (e.g. Python)  
- External tooling makes the system less accessible and harder to debug  
- Hard to later support dynamic/datalogger parameters in the same manager  

---

### **Option 2: Self-Registration (Chosen)**

Each parameter registers itself inside its constructor:

```cpp
Parameters::instance().registerParam(this);
```

The `ParameterManager` maintains a runtime array/list of parameter pointers in RAM.

### **Pros**
- **High modularity:** parameters can be defined directly in the file/module that implements the signal  
- No central registration file to maintain  
- No pre-build or code-generation scripts required  
- Easy to add or remove parameters by editing only the component’s header  
- Naturally extensible to dynamic parameters (future datalogging use cases)  
- Debugging is simpler because definition and registration are tightly coupled  

### **Cons**
- Requires a registry in **RAM**: typically one pointer per parameter (~4 bytes on 32-bit MCUs)  
- Constructor introduces a global side effect  
- Initialization order must be understood (but is deterministic for static objects)  

---

## **Final Decision**

**Use self-registration for all parameters in the embedded firmware.**

This choice provides the best balance of modularity, maintainability, and future extensibility.  
A fully static parameter registry stored only in Flash would remove the RAM cost of the registration list,  
but would also require a centralized master list or external code-generation tools — making the system  
harder to maintain, harder to debug, and less accessible to contributors.

---

# 🧭 Design Decision: Parameter Definitions (Avoiding Double Definitions)

## **Overview**

To keep parameter definitions clean and avoid redundant metadata declarations,  
a macro-based approach is used. This allows defining each parameter in **one line**,  
with metadata stored in Flash and automatic self-registration.

This ensures:

- No repeated values (name, id, ranges, categories, etc.)
- Metadata and parameter instances stay in sync
- Minimal boilerplate code

---

## ✔ **Recommended Static Parameter Macro (One Line)**

```cpp
#define PARAM(type, varname, id, name, unit, cat, minv, maxv, def) \
    static const ParamDesc<type> desc_##varname { id, name, unit, cat, minv, maxv, def }; \
    namespace params { static StaticParameter<type> varname{ desc_##varname }; }
```

### **Advantages**

- **Single source of truth**  
- Clean `params::varname` namespace access  
- Metadata stored in **Flash** (`static const ParamDesc`)  
- Parameter objects **self-register automatically**  
- No duplicate definitions  
- Works seamlessly with `StaticParameter<T>` or subclass implementations  

---

