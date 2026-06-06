mkdir -p tests/valids tests/invalids tests/bin
ASTRA=./build/src/astra

1. 
cat > tests/valids/hello.astra <<'ASTRA'
fn main() -> int32 {
    print("Hello, Astra");
    return 0;
}
ASTRA

$ASTRA tests/valids/hello.astra -o tests/bin/hello
tests/bin/hello
answer: Hello, Astra 
echo "Код возврата: $?"
answer: Код возврата: 0

2. 
cat > tests/valids/math.astra <<'ASTRA'
fn main() -> int32 {
    let value: int32 = 2 + 3 * 4;
    let grouped: int32 = (2 + 3) * 4;

    print(value);
    print(grouped);

    return value;
}
ASTRA

$ASTRA tests/valids/math.astra -o tests/bin/math
tests/bin/math
answer: 
14
20
echo "Код возврата: $?"
answer: Код возврата: 14

3. 
cat > tests/valids/branch.astra <<'ASTRA'
fn main() -> int32 {
    let x: int32 = 7;

    if (x > 5 && true) {
        print("big");
        return 1;
    } else {
        print("small");
        return 0;
    }
}
ASTRA

$ASTRA tests/valids/branch.astra -o tests/bin/branch
tests/bin/branch
answer: big
echo "Код возврата: $?"
answer: Код возврата: 1

4. 
cat > tests/valids/loop.astra <<'ASTRA'
fn main() -> int32 {
    var i: int32 = 0;
    var sum: int32 = 0;

    while (i < 5) {
        i = i + 1;

        if (i == 3) {
            continue;
        }

        sum = sum + i;

        if (sum > 7) {
            break;
        }
    }

    print(sum);
    return sum;
}
ASTRA

$ASTRA tests/valids/loop.astra -o tests/bin/loop
tests/bin/loop
answer : 12
echo "Код возврата: $?"
answer: 12

5. 
cat > tests/valids/functions.astra <<'ASTRA'
fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

fn mul(a: int32, b: int32) -> int32 {
    return a * b;
}

fn main() -> int32 {
    let result: int32 = mul(add(2, 3), 4);

    print(result);

    return result;
}
ASTRA

$ASTRA tests/valids/functions.astra -o tests/bin/functions
tests/bin/functions
answer: 20
echo "Код возврата: $?"
Код возврата: 20

6. 
cat > tests/valids/values.astra <<'ASTRA'
fn main() -> int32 {
    let text: string = "test";
    let letter: char = 'A';
    let ok: bool = true;

    print(text);
    print(letter);
    print(ok);

    return 0;
}
ASTRA

$ASTRA tests/valids/values.astra -o tests/bin/values
tests/bin/values
answer: 
test
A
true
echo "Код возврата: $?"
answer: Код возврата: 0


7. 
cat > tests/valids/array.astra <<'ASTRA'
fn main() -> int32 {
    var values: [int32; 3] = [10, 20, 30];

    print(len(values));
    print(values[1]);

    values[2] = values[0] + values[1];

    print(values[2]);

    return values[2];
}
ASTRA

$ASTRA tests/valids/array.astra -o tests/bin/array
tests/bin/array
answer: 
3
20
30
echo "Код возврата: $?"
answer: Код возврата: 30


8. 
cat > tests/valids/point.astra <<'ASTRA'
struct Point {
    x: int32;
    y: int32;
}

fn main() -> int32 {
    var p: Point = Point { x: 3, y: 4 };

    print(p.x);

    p.y = p.y + 1;

    print(p.y);

    return p.x + p.y;
}
ASTRA

$ASTRA tests/valids/point.astra -o tests/bin/point
tests/bin/point
3
5
echo "Код возврата: $?"
Код возврата: 8

9. 
cat > tests/valids/alias.astra <<'ASTRA'
type Score = int32;

fn main() -> int32 {
    let score: Score = 100;

    print(score);

    return score;
}
ASTRA

$ASTRA tests/valids/alias.astra -o tests/bin/alias
tests/bin/alias
100
echo "Код возврата: $?"
Код возврата: 100

10. 
cat > tests/valids/cast.astra <<'ASTRA'
fn main() -> int32 {
    let a: int32 = 65;
    let big: int64 = a as int64;
    let value: float64 = a as float64;

    print(big);
    print(value);

    return a;
}
ASTRA

$ASTRA tests/valids/cast.astra -o tests/bin/cast
tests/bin/cast
65
65
echo "Код возврата: $?"
Код возврата: 127

11. 
cat > tests/valids/check.astra <<'ASTRA'
fn main() -> int32 {
    let name: string = "astra";

    assert(len(name) == 5);

    print(len(name));

    return len(name);
}
ASTRA

$ASTRA tests/valids/check.astra -o tests/bin/check
tests/bin/check
5
echo "Код возврата: $?"
Код возврата: 5

12. 
cat > tests/valids/comment.astra <<'ASTRA'
fn main() -> int32 {
    // Simple line comment.
    let x: int32 = 10;

    // Another line comment.
    print(x);

    return x;
}
ASTRA

$ASTRA tests/valids/comment.astra -o tests/bin/comment
tests/bin/comment
10
echo "Код возврата: $?"
Код возврата: 10

13. 
cat > tests/invalids/no_main.astra <<'ASTRA'
fn helper() -> int32 {
    return 0;
}
ASTRA

$ASTRA tests/invalids/no_main.astra -o tests/bin/no_main
answer: tests/invalids/no_main.astra:1:1: error: программа должна содержать функцию main
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

14. 
cat > tests/invalids/bad_type.astra <<'ASTRA'
fn main() -> int32 {
    let x: int32 = "text";
    return 0;
}
ASTRA

$ASTRA tests/invalids/bad_type.astra -o tests/bin/bad_type
answer: tests/invalids/bad_type.astra:2:20: error: несовместимые типы: ожидается int32, получен string 
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

15. 
cat > tests/invalids/unknown_name.astra <<'ASTRA'
fn main() -> int32 {
    return value;
}
ASTRA

$ASTRA tests/invalids/unknown_name.astra -o tests/bin/unknown_name
answer: tests/invalids/unknown_name.astra:3:12: error: неизвестное имя 'value'
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

16. 
cat > tests/invalids/const_assign.astra <<'ASTRA'
fn main() -> int32 {
    let x: int32 = 1;

    x = 2;

    return x;
}
ASTRA

$ASTRA tests/invalids/const_assign.astra -o tests/bin/const_assign
answer: tests/invalids/const_assign.astra:4:5: error: нельзя присваивать иммутабельной переменной
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

17. 
cat > tests/invalids/break_error.astra <<'ASTRA'
fn main() -> int32 {
    break;
    return 0;
}
ASTRA

$ASTRA tests/invalids/break_error.astra -o tests/bin/break_error
answer: tests/invalids/break_error.astra:2:5: error: break вне цикла
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

18. 
cat > tests/invalids/args.astra <<'ASTRA'
fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

fn main() -> int32 {
    return add(1);
}
ASTRA

$ASTRA tests/invalids/args.astra -o tests/bin/args
answer: tests/invalids/args.astra:6:15: error: функция 'add' ожидает 2 аргументов, получено 1
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

19. 
cat > tests/invalids/index.astra <<'ASTRA'
fn main() -> int32 {
    var values: [int32; 2] = [1, 2];

    return values[true];
}
ASTRA

$ASTRA tests/invalids/index.astra -o tests/bin/index
answer: tests/invalids/index.astra:4:19: error: индекс массива должен быть целым числом, получен bool
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

20. 
cat > tests/invalids/field.astra <<'ASTRA'
struct Point {
    x: int32;
}

fn main() -> int32 {
    let p: Point = Point { x: 1 };

    return p.y;
}
ASTRA

$ASTRA tests/invalids/field.astra -o tests/bin/field
answer: tests/invalids/field.astra:8:13: error: поле 'y' не существует в Point
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

21. 
cat > tests/invalids/return_type.astra <<'ASTRA'
fn main() -> int32 {
    return "bad";
}
ASTRA

$ASTRA tests/invalids/return_type.astra -o tests/bin/return_type
answer: tests/invalids/return_type.astra:2:12: error: несовместимые типы: ожидается int32, получен string
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1

22. 
cat > tests/invalids/condition.astra <<'ASTRA'
fn main() -> int32 {
    if (123) {
        return 1;
    }

    return 0;
}
ASTRA

$ASTRA tests/invalids/condition.astra -o tests/bin/condition
answer: tests/invalids/condition.astra:2:9: error: условие if должно быть bool, получен int32
echo "Код возврата компилятора: $?"
Код возврата компилятора: 1