package org.example

import org.test.Message as TestMessage

fun double(x: Int): Int {
    return 2 * x
}

val d2 = double(2)

fun <T> asList(vararg ts: T): List<T> {
    val result = ArrayList<T>()
    for (t in ts)
        result.add(t)
    return result
}

class Person(val name: String) {
    var age: Int = 0
}

enum class Direction {
    NORTH, SOUTH, WEST, EAST
}

enum class Color(val rgb: Int) {
    RED(0xFF0000),
    GREEN(0x00FF00),
    BLUE(0x0000FF)
}

enum class ProtocolState {
    WAITING {
        override fun signal() = TALKING
    },

    TALKING {
        override fun signal() = WAITING
    };

    abstract fun signal(): ProtocolState
}

interface MyInterface {
    fun bar()
    fun foo() {
      // optional body
    }
}

object Obj {
    const val ECT: String = "asdf"
}

class MyClass {
    companion object Factory {
        fun create(): MyClass = MyClass()
    }
}

class User
@AssistedInject
constructor(
    val name: String,
    private val intToString: (Int) -> String,
    @Assisted private var params: Params?,
) {
    companion object {
        private val defaultGreeting = "Hello"
    }

    fun sayHi() {
        println(defaultGreeting)
    }
}

class Outer {
    sealed class Inner {
        abstract val blah: Boolean
        data object ObjO : Inner() {
            override val blah get() = false
        }
        data class ClaP(override val blah: Boolean) : Inner()
        data class ClaQ(val i: Int, override val blah: Boolean) : Inner()
    }

    private val thing = 0


    private fun createSalad(): Salad =
        object : SaladWrapper() {
            override fun getTomaeto(): Int = zxcv.tomaeto
            override fun getTomahto(): Int = zxcv.tomahto
        }
}

val d3 = double(3)
