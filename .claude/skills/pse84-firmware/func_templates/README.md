# func_templates - services, middleware, custom logic

Reusable logic on top of bsp APIs: console, LED/button services, sensor processing, LVGL UI,
connectivity clients, ML pre/post-processing, application algorithms. No `CYBSP_*` / `Cy_*` pin
code here. Declare the execution models a component works under (`exec_models`) and the bsp
components it needs. Schema and workflow: `../reference/components.md`; catalog: `docs/components.md`.
