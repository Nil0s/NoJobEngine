# RLTrackingAgent: corrección y validación

Aplicar el ZIP sobre la raíz del proyecto v2.1Not, conservando Assets/, Engine/ y Editor/. Contiene únicamente archivos nuevos o modificados. Recompilar el editor y los scripts del proyecto; reiniciar Play para cargar la política incluida.

## Cambios

- El agente observa la posición actual del objetivo en X y Z en cada decisión. La versión anterior solo procesaba X; también reiniciaba el agente al llegar o después de 40 pasos.
- AutoReset ahora es false en el código y en RLTrackingDemo. Permanece junto al objetivo y reanuda el seguimiento cuando se desplaza. AutoReset=true conserva un modo episódico con reinicio de ambos ejes.
- Se reutiliza una política DQN de 2 entradas y 3 acciones de forma independiente por eje. No es una red conjunta de cuatro entradas. TrackingPolicy.h es compartido por el runtime y la prueba.
- AI Debugger incluye RLTrackingAgent en sus dos filtros y consulta el snapshot virtual de la instancia runtime, también a través de la DLL de scripts.
- Observaciones: [X, targetX-X, Z, targetZ-Z], normalizadas. Q: [-X, hold X, +X, -Z, hold Z, +Z]. El estado muestra las dos acciones aplicadas y los indicadores de hold. Los valores Q son números crudos, no probabilidades; las barras visuales saturan fuera de [-1,1]. Las observaciones/Q corresponden al instante anterior al movimiento y la distancia al posterior. No se dibuja una falsa red conjunta de cuatro entradas.
- Se incluye una política entrenada desde cero mediante Double DQN, sin etiquetas de dirección ni pesos escritos a mano.

## Prueba reproducible sin dependencias gráficas

Desde la raíz del proyecto, con CMake >=3.25 y un compilador C++20:

```sh
cmake -S Engine/AI/Reinforcement -B build/rl-proof
cmake --build build/rl-proof --config Release
ctest --test-dir build/rl-proof -C Release -V --output-on-failure
```

El subproyecto es autónomo y no descarga las dependencias del editor. Mantiene habilitados los asserts de ReinforcementTests incluso en Release. TrackingLearningProof usa comprobaciones con códigos de salida, independientes de los asserts.

TrackingLearningProof entrena 1.600 episodios (semilla 42). Evalúa 500 escenarios reservados (semillas 100000..100499), con dos objetivos sucesivos por escenario, 40 decisiones por objetivo y dos ejes. La segunda fase conserva la posición final de la primera: obliga a volver a decidir tras mover el objetivo. La política aleatoria tiene semilla 90210 y exactamente los mismos límites y regla de hold. Métrica: fracción de las 1.000 fases que alcanzan |dx|<0.08 y |dz|<0.08 antes de agotar el presupuesto.

Aceptación: entrenada >=90%, y al menos 30 puntos porcentuales sobre aleatorio y sobre la red sin entrenar. Se comprueba además paridad tras guardar/recargar y respuesta hacia ambos lados al cambiar un objetivo desde la misma posición.

Resultado ejecutado en Windows x64, MSVC 19.51, Release:

| Política | Fases con éxito |
|---|---:|
| Sin entrenar | 0,6% |
| Aleatoria | 22,0% |
| DQN entrenada | 100,0% |

CTest: 2/2 pruebas aprobadas. El informe completo está en TRACKING_TEST_RESULTS.txt. El ejecutable guarda por defecto RLTrackingPolicy.njn en su directorio de trabajo; CTest no sobrescribe el asset del proyecto. Para regenerarlo deliberadamente, ejecutar el binario indicando Assets/Neural/RLTrackingPolicy.njn como argumento desde la raíz del proyecto. Otros compiladores pueden producir resultados numéricos distintos; los umbrales se verifican al ejecutar.

## Verificación manual pendiente en el editor

1. Abrir Assets/Scenes/RLTrackingDemo.nojobscene y comprobar que el suelo usa Assets/Models/testplane_100x100.obj. Hacer el bake con ese modelo si se quiere comprobar el sistema de navegación. El plano, su escala y sus metadatos no se han modificado.
2. Compilar/recargar scripts y entrar en Play. Con AutoReset=false, esperar a que el cubo azul alcance al naranja en X/Z.
3. Abrir AI Debugger y seleccionar RLAgentCube. En Neural Inspector - Live deben aparecer cuatro observaciones, seis Q y acciones X/Z. Behavior State debe reflejar tracking/reached.
4. Mover la instancia runtime de RLTarget a (-5, 0.75, 5), luego a (5, 0.75, -5). Debe retomar el movimiento después del siguiente intervalo de decisión y aumentar arrivals una vez por llegada. Moverlo también solo en Z.
5. Mantener el objetivo quieto: el agente debe permanecer dentro de la tolerancia. Eliminar el objetivo: Behavior State debe indicar target missing y el snapshot anterior dejar de publicarse.

## Limitaciones declaradas

No se ha compilado ni ejecutado el editor completo, la DLL de scripts ni el bake; se validaron la red nativa, DQN, el helper compartido, el guardado/recarga y el subproyecto CMake. La integración visual requiere los pasos anteriores. No se afirma que la interfaz haya sido probada visualmente.

El control es directo sobre Transform, sin NavAgent, colisiones ni búsqueda de rutas. No utiliza el bake para tomar decisiones. El modelo testplane_100x100.obj conserva exactamente sus bytes originales y sigue siendo el único suelo de la escena de demo.

Dominio soportado: X/Z entre -WorldScale y +WorldScale (por defecto -8..8), Y constante. Fuera del dominio se recorta el objetivo para inferencia y se avisa en el estado; no se promete alcanzarlo. Dentro de 0.08 normalizados por eje (0.64 unidades con escala 8), se aplica hold explícito para estabilizar la llegada, tanto al agente como a los comparadores. Ese hold no es aprendizaje. Cada movimiento es 0.12 normalizados por eje (0.96 unidades); las diagonales son más rápidas. Como antes, se realiza como máximo una decisión por frame al cumplirse ActionInterval, sin recuperar pasos perdidos durante frames lentos.

La prueba demuestra aprendizaje en seguimiento separable X/Z con objetivos reubicados, no navegación general, obstáculos, aprendizaje online durante Play ni seguimiento de cualquier trayectoria o velocidad. Se usa una semilla de entrenamiento: no es una estimación estadística multisemilla. El panel de RL original sigue entrenando el entorno de un eje; el mismo modelo se reutiliza en ambos ejes. Para reproducir exactamente la política entregada, usar TrackingLearningProof.
