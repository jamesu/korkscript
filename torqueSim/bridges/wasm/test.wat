(module
  ;; --- Import from host: print(offset: i32) -> void
  (import "env" "print" (func $print (param i32)))

  ;; --- Linear memory: fixed at 2 pages (128 KiB) ---
  (memory (export "memory") 2 2)

  ;; --- Data segment: static string ---
  (data (i32.const 16) "Hello World\00")

  ;; --- Heap layout & globals ---
  (global $heap_base i32 (i32.const 1024))
  (global $heap_end  i32 (i32.const 131072)) ;; 2 * 64 KiB
  (global $heap_ptr (mut i32) (i32.const 1024))
  (global $free_head (mut i32) (i32.const 0))

  ;; --- Helpers ---
  (func $align_8 (param $n i32) (result i32)
    local.get $n
    i32.const 7
    i32.add
    i32.const -8
    i32.and)

  (func $push_free (param $blk i32)
    local.get $blk
    global.get $free_head
    i32.store offset=4
    local.get $blk
    global.set $free_head)

  (func $unlink (param $prev i32) (param $cur i32)
    (local $next i32)
    local.get $cur
    i32.load offset=4
    local.set $next
    local.get $prev
    i32.eqz
    if
      local.get $next
      global.set $free_head
    else
      local.get $prev
      local.get $next
      i32.store offset=4
    end)

  ;; --- Arithmetic ---
  (func (export "add") (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.add)

  (func (export "sub") (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.sub)

  ;; --- malloc ---
  (func (export "malloc") (param $n i32) (result i32)
    (local $need i32) (local $total i32) (local $prev i32) (local $cur i32)
    (local $sz i32) (local $hp i32) (local $newhp i32) (local $rem i32) (local $rem_blk i32)

    ;; handle n <= 0
    local.get $n
    i32.const 0
    i32.le_s
    if
      i32.const 0
      return
    end

    ;; sizes
    local.get $n
    call $align_8
    local.set $need
    local.get $need
    i32.const 8
    i32.add
    local.set $total

    ;; Wrap search in an outer block so we can br to the bump path.
    block $bump_alloc
      ;; init free list scan
      i32.const 0
      local.set $prev
      global.get $free_head
      local.set $cur

      loop $search
        ;; if cur == 0 → jump to bump_alloc
        local.get $cur
        i32.eqz
        if
          br $bump_alloc
        end

        ;; load size of current block
        local.get $cur
        i32.load
        local.set $sz

        ;; fit?
        local.get $sz
        local.get $total
        i32.ge_u
        if
          ;; unlink from free list
          local.get $prev
          local.get $cur
          call $unlink

          ;; compute remainder
          local.get $sz
          local.get $total
          i32.sub
          local.set $rem

          ;; split if remainder >= 16 (header+min payload)
          local.get $rem
          i32.const 16
          i32.ge_u
          if
            local.get $cur
            local.get $total
            i32.add
            local.set $rem_blk

            local.get $rem_blk
            local.get $rem
            i32.store

            local.get $rem_blk
            call $push_free

            local.get $cur
            local.get $total
            i32.store
          else
            local.get $cur
            local.get $sz
            i32.store
          end

          ;; return payload
          local.get $cur
          i32.const 8
          i32.add
          return
        end

        ;; advance
        local.get $cur
        local.set $prev
        local.get $cur
        i32.load offset=4
        local.set $cur
        br $search
      end

      ;; --- bump allocation path (runs when we br $bump_alloc) ---
      global.get $heap_ptr
      local.set $hp
      local.get $hp
      local.get $total
      i32.add
      local.set $newhp

      local.get $newhp
      global.get $heap_end
      i32.gt_u
      if
        i32.const 0
        return
      end

      local.get $hp
      local.get $total
      i32.store
      local.get $newhp
      global.set $heap_ptr
      local.get $hp
      i32.const 8
      i32.add
      return
    end ;; end block $bump_alloc

    ;; We should never fall through here, but satisfy the validator:
    i32.const 0
    return
  )

  ;; --- free ---
  (func (export "free") (param $p i32)
    (local $blk i32)
    local.get $p
    i32.eqz
    if
      return
    end
    local.get $p
    i32.const 8
    i32.sub
    local.set $blk
    local.get $blk
    call $push_free)

  ;; --- testPrint ---
  (func (export "testPrint")
    i32.const 16    ;; offset of "Hello World"
    call $print)
)
