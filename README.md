# L05: Debugging Básico mediante Instrumentación

## Introducción

Este laboratorio introduce una primera metodología de debugging en C basada en instrumentar el código fuente para volver observable el estado interno del programa mientras se ejecuta. Aquí el foco está en técnicas simples y transferibles: agregar prints, activar y desactivar trazas de depuración, acotar la información que se imprime dentro de ciclos, fijar semillas para reproducir comportamiento aleatorio y usar `assert` para validar supuestos del programa.

El laboratorio se apoya en un programa inicial que recibe parámetros numéricos. El código compila correctamente, pero contiene múltiples bugs lógicos intencionales distribuidos en distintas funciones.

### Pre-requisitos

- Tener iniciada la máquina virtual Lubuntu y clonar este repositorio
- Tener disponible `gcc`, `diff` y `chmod`
- Haber trabajado previamente con terminal, edición en `vim` y compilación en C
- Haber completado el laboratorio anterior sobre automatización de pruebas

### Objetivo general

- Depurar un programa en C mediante instrumentación del código fuente, utilizando prints, flags de depuración, semillas fijas y `assert`, guiándose por una batería sistemática de pruebas.

### Objetivos específicos

- Ejecutar una suite de pruebas automatizadas para detectar fallos reproducibles
- Agregar prints de progreso para seguir el flujo general del programa
- Agregar prints de estado para observar variables internas relevantes
- Usar `stderr` para depuración sin contaminar la salida normal del programa
- Activar y desactivar trazas de depuración mediante una bandera de compilación
- Limitar la salida de debug mediante prints condicionales dentro de ciclos
- Reproducir el mismo comportamiento aleatorio usando semillas fijas
- Incorporar `assert` para validar invariantes internos del programa
- Corregir bugs lógicos en base a evidencia observada durante la ejecución

### Metodología

A lo largo de este laboratorio se trabajará sobre un programa relativamente largo, dividido en varias funciones, con una simulación paso a paso y una pequeña componente aleatoria. El programa es más complejo que lo que un estudiante debería dominar a esta altura de la asignatura, esto es intencional ya que la idea es que el estudiante no pueda resolver el problema solo leyendo el código, sino que deba instrumentarlo de manera sistemática para entender qué está ocurriendo. Primero se compila el programa y se ejecuta manualmente para entender su interfaz. Luego se ejecuta una suite automatizada de pruebas, lo que permite observar que múltiples casos fallan. A partir de ahí el trabajo no consiste en adivinar el error leyendo el código completo, sino en instrumentar el programa para responder preguntas concretas: cuánto avanza, qué valores toman las variables relevantes, cuándo se desvían, y qué condiciones determinan el resultado final.

### Estructura del repositorio

El laboratorio utiliza tres carpetas en paralelo: `code`, `tests` y `scripts`.

- La carpeta `code` contiene el programa `torpedos_protones.c`.
- La carpeta `tests` contiene archivos `testNNN.in` y `testNNN.expected` en una única carpeta plana. En este laboratorio los `.expected` verifican solo el resultado principal de la misión, no todos los valores numéricos intermedios.
- La carpeta `scripts` contiene scripts para ejecutar y limpiar pruebas con el mismo flujo introducido en L04.

### Contexto

Durante el ataque final contra la Estrella de la Muerte, la Alianza Rebelde debe lanzar un torpedo de protones dentro de un conducto estrecho para alcanzar un punto crítico del reactor. En este laboratorio se entrega un simulador de esa maniobra, el cual trabaja con un modelo físico básico pero suficiente para observar cómo evoluciona una trayectoria del torpedo de protones en pasos discretos.

En cada misión, el programa genera internamente una semilla pseudo-aleatoria al comenzar la ejecución. Esa semilla controla la turbulencia de la simulación, por lo que dos ejecuciones consecutivas del mismo caso pueden no comportarse exactamente igual. Además de esa semilla interna, el programa recibe por `stdin` seis valores, uno por línea:

1. distancia al objetivo
2. velocidad de avance por paso
3. ángulo inicial del disparo
4. desviación lateral inicial
5. intensidad del sistema de corrección
6. tolerancia máxima aceptable al impactar el objetivo

Con esos datos el programa simula paso a paso el avance del torpedo por el conducto. En cada iteración se combinan tres efectos:

- una deriva inicial asociada al ángulo del disparo
- una corrección lateral que intenta acercar el torpedo al centro
- una pequeña turbulencia aleatoria

Al finalizar la simulación, el programa reporta una salida estructurada con el resultado de la misión, la cantidad de pasos, la distancia alcanzada, la desviación final y la estabilidad del torpedo. Sin embargo, el código entregado contiene bugs lógicos intencionales. Algunos producen desviaciones incorrectas, otros alteran el resultado final solo en ciertos casos de borde.

## Actividad

### 1. Compilar el programa

Entrar a la carpeta `code` y compilar manualmente con `gcc`:

```bash
cd code
gcc -Wall -std=c11 -o torpedos_protones torpedos_protones.c
```

Esto genera el ejecutable `torpedos_protones`.

### 2. Ejecutar el programa manualmente

Antes de correr pruebas automatizadas, conviene entender la interfaz del programa ejecutando una misión manualmente. Por ejemplo:

```bash
./torpedos_protones << EOF
100
10
2
0
0
1.00
EOF
```

La salida del programa tiene la siguiente forma general:

```text
Mission        : SUCCESS|FAILURE
Steps          : <n>
Final distance : <value>
Final offset   : <value>
Final stability: <value>
Reason         : <description>
```

La ejecución es aparentemente razonable, pero eso no garantiza que el programa esté correcto. Para eso necesitamos una forma sistemática de comprobar múltiples casos.

### 3. Ejecutar todos los tests

Este laboratorio reutiliza el mismo flujo de pruebas introducido en L04. Primero otorgar permisos de ejecución a los scripts:

```bash
chmod +x scripts/tests-run.sh
chmod +x scripts/tests-clean.sh
```

Donde `tests-run.sh` es un script que recibe dos parámetros: la ruta al programa a probar y la ruta a la carpeta de tests de la siguiente forma:

```bash
./scripts/tests-run.sh ./code/torpedos_protones ./tests
```

Al desarrollar esta exploración de casos más completa, notamos una mezcla de comportamientos: los primeros cuatro tests pasan, `test005` y `test006` dan error, `test008` a `test010` fallan, y `test007` hay veces que pasa y veces que falla.

Cuando sea necesario eliminar los archivos `.out`, `.err` y `.diff` generados por el script, usar:

```bash
./scripts/tests-clean.sh ./tests
```

### 4. Prints de debug

Cuando un programa es largo y tiene muchas funciones, lo primero no es imprimir todas las variables, sino responder preguntas simples de flujo:

- se está ejecutando realmente el ciclo principal
- cuántas iteraciones alcanza a completar
- el torpedo llega al objetivo o abandona antes el conducto
- qué función parece estar tomando una decisión incorrecta

Para eso se pueden agregar mensajes simples en puntos estratégicos, por ejemplo al inicio de cada paso de simulación o justo antes de evaluar el resultado final. En una primera aproximación, incluso se puede partir con ejemplos tan simples como estos:

```c
printf("starting step %d\n", step);
fprintf(stderr, "offset before correction = %.2f\n", offset);
```

En el primer caso `printf(...)` se usa para mostrar un mensaje de progreso que se muestra en la salida estándar, mientras que en el segundo caso `fprintf(stderr, ...)` se usa para mostrar un valor relevante en la salida de error sin contaminar la salida normal del programa. El usar `printf` directamente puede ser fácil y simple para diagnosticar casos puntuales, pero eso contamina la salida del programa, lo que puede romper la comparación con los archivos `.expected` de la suite de pruebas. Por eso es recomendable usar `stderr` para los mensajes de depuración, dejando `stdout` limpio para la salida funcional del programa.

### 5. Wrapper para los prints de debug

Usar `fprintf(stderr, ...)` es una solución útil, pero no es la única. Otra alternativa es encapsular los prints de debug y controlar desde un solo lugar si se imprimen o no. En este enfoque los mensajes pueden seguir saliendo por `stdout` durante la revisión manual y/o `stderr`, y luego suprimirse por completo en la compilación que se use para las pruebas automatizadas.

Una opción simple es esta:

```c
#include <stdio.h>

int debug_enabled = 0;

void debug_printf(const char *label, int step, float offset)
{
    if (debug_enabled)
    {
        printf("[%s] step=%d offset=%.2f\n", label, step, offset);
    }
}
```

Pero obliga a que cada print de debug tenga la misma firma, lo que puede no ser tan flexible. Otra opción es usar una función con firma variable, aunque eso complica un poco la implementación:

```c
#include <stdio.h>
#include <stdarg.h>

int debug_enabled = 0;

void debug_printf(const char *format, ...)
{
    if (debug_enabled)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}
```

También se puede usar la misma idea sin una función separada, dejando cada print encapsulado por la condición:

```c
#include <stdio.h>

int debug_enabled = 0;

if (debug_enabled)
{
    printf("step=%d offset=%.2f\n", step, offset);
}
```

La ventaja de esto es que para activar o desactivar la salida de debug basta cambiar el valor de `debug_enabled` o decidir, al compilar, si se usará una versión pensada para inspección manual o una versión limpia para ejecutar la suite de pruebas.

De esta manera:

- todo el debug queda centralizado en un único punto de control
- se puede preparar una compilación con trazas para revisar casos manualmente
- se puede preparar otra compilación sin trazas para que el script compare solo la salida funcional del programa

### 6. Prints condicionales dentro del ciclo

Si se imprime el estado completo en cada iteración, la salida puede volverse inmanejable. Por eso es útil imprimir solo en casos relevantes. Por ejemplo:

- cuando la desviación supera cierto umbral
- cada cierta cantidad de iteraciones
- cuando la estabilidad cae bajo un valor sospechoso
- justo antes de abandonar el conducto

Este tipo de trazas suele ser más informativo que imprimir todo en todo momento.

### 7. Semillas fijas y reproducibilidad


Para la generación de números aleatorios, los sistemas informáticos no generan números verdaderamente aleatorios, sino que usan algoritmos que producen secuencias de números pseudoaleatorios a partir de una semilla inicial. Si esa semilla es la misma, la secuencia generada también será la misma.

En le caso de nuestro programa, la semilla se genera internamente al iniciar la ejecución, lo que hace que cada misión pueda comportarse de forma diferente incluso con los mismos parámetros de entrada. Esto puede dificultar la depuración, ya que un bug puede manifestarse solo con ciertas semillas. Esto se vuelve especialmente crítico en el test007, que fue diseñado para ser sensible a la semilla.

Para controlar que el comportamiento de la simulación sea reproducible, durante el laboratorio conviene mantener la semilla constante mientras se estudia un caso. Solo después de corregir un bug tiene sentido probar con otras semillas para verificar que la solución no depende de una ejecución particular. Es importante eso si tener claro que semilla se utilizó en cada caso, para poder volver a reproducirlo en caso que se detecte un error que no se había observado antes.

### 8. Quinto nivel: usar `assert` para validar invariantes

Los `assert` no sirven para validar errores esperados del usuario. Su utilidad está en comprobar condiciones internas que el programador cree verdaderas. Ejemplos razonables para este programa podrían ser:

- la velocidad debe ser positiva
- la tolerancia debe ser positiva
- la distancia recorrida no debería decrecer
- el número de pasos no debería exceder un límite razonable
- la simulación no debería seguir avanzando si el torpedo ya no está activo

Agregar `assert` en lugares estratégicos ayuda a detener la ejecución exactamente cuando una suposición interna deja de cumplirse.

### 9. Orden sugerido para corregir los bugs

Los tests fueron preparados en el mismo orden en que conviene estudiar el programa. La sugerencia es avanzar de forma incremental, ejecutando la suite completa después de cada cambio:

- `test001` a `test004`: ejemplos de referencia que ya deberían pasar y sirven para verificar que no se rompa el comportamiento básico mientras se corrigen otros problemas
- `test005` y `test006`: errores iniciales fáciles de detectar, asociados a lectura y validación de la configuración
- `test007`: caso deliberadamente sensible a la semilla, útil para notar que sin control de la pseudo-aleatoriedad el comportamiento puede parecer incierto incluso dentro de la suite automatizada
- `test008`: caso donde conviene inspeccionar el cálculo de la trayectoria y la corrección lateral dentro del ciclo
- `test009` y `test010`: casos de borde para verificar la condición final de impacto

La idea no es arreglar todo a la vez, sino localizar un problema, corregirlo, volver a correr los tests y usar el nuevo estado de la suite para decidir el siguiente paso.
