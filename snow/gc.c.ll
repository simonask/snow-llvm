; ModuleID = 'gc.c.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64"
target triple = "x86_64-apple-darwin10.0.0"

%struct.SnArguments = type opaque
%struct.SnJIT = type opaque
%struct.SnObject = type { %struct.SnType*, %struct.SnProcess*, i8* }
%struct.SnProcess = type { %struct.SnJIT* }
%struct.SnType = type { i64, void (%struct.SnProcess*, %struct.SnObject*)*, void (%struct.SnProcess*, %struct.SnObject*)*, void (%struct.SnProcess*, %struct.SnObject*, %struct.SnObject*)*, void (%struct.SnProcess*, %struct.SnObject*, void (%struct.SnProcess*, %struct.SnObject*, i64*, i8*)*, i8*)*, i64 (%struct.SnProcess*, %struct.SnObject*, i64)*, i64 (%struct.SnProcess*, %struct.SnObject*, i64, i64)*, i64 (%struct.SnProcess*, %struct.SnObject*, i64, %struct.SnArguments*)* }

define void @snow_gc(%struct.SnProcess*) nounwind ssp {
entry:
  %.addr = alloca %struct.SnProcess*, align 8     ; <%struct.SnProcess**> [#uses=1]
  store %struct.SnProcess* %0, %struct.SnProcess** %.addr
  br label %do.body

do.body:                                          ; preds = %entry
  call void asm sideeffect "int3\0Anop\0A", "~{dirflag},~{fpsr},~{flags}"() nounwind
  br label %do.end

do.end:                                           ; preds = %do.body
  ret void
}

define %struct.SnObject* @snow_gc_create_object(%struct.SnProcess* %p, %struct.SnType* %type) nounwind ssp {
entry:
  %retval = alloca %struct.SnObject*, align 8     ; <%struct.SnObject**> [#uses=2]
  %p.addr = alloca %struct.SnProcess*, align 8    ; <%struct.SnProcess**> [#uses=3]
  %type.addr = alloca %struct.SnType*, align 8    ; <%struct.SnType**> [#uses=4]
  %mem = alloca i8*, align 8                      ; <i8**> [#uses=3]
  %obj = alloca %struct.SnObject*, align 8        ; <%struct.SnObject**> [#uses=6]
  store %struct.SnProcess* %p, %struct.SnProcess** %p.addr
  store %struct.SnType* %type, %struct.SnType** %type.addr
  %tmp = load %struct.SnType** %type.addr         ; <%struct.SnType*> [#uses=1]
  %tmp1 = getelementptr inbounds %struct.SnType* %tmp, i32 0, i32 0 ; <i64*> [#uses=1]
  %tmp2 = load i64* %tmp1                         ; <i64> [#uses=1]
  %add = add i64 24, %tmp2                        ; <i64> [#uses=1]
  %call = call i8* @malloc(i64 %add)              ; <i8*> [#uses=1]
  store i8* %call, i8** %mem
  %tmp4 = load i8** %mem                          ; <i8*> [#uses=1]
  %0 = bitcast i8* %tmp4 to %struct.SnObject*     ; <%struct.SnObject*> [#uses=1]
  store %struct.SnObject* %0, %struct.SnObject** %obj
  %tmp5 = load %struct.SnType** %type.addr        ; <%struct.SnType*> [#uses=1]
  %tmp6 = load %struct.SnObject** %obj            ; <%struct.SnObject*> [#uses=1]
  %tmp7 = getelementptr inbounds %struct.SnObject* %tmp6, i32 0, i32 0 ; <%struct.SnType**> [#uses=1]
  store %struct.SnType* %tmp5, %struct.SnType** %tmp7
  %tmp8 = load %struct.SnProcess** %p.addr        ; <%struct.SnProcess*> [#uses=1]
  %tmp9 = load %struct.SnObject** %obj            ; <%struct.SnObject*> [#uses=1]
  %tmp10 = getelementptr inbounds %struct.SnObject* %tmp9, i32 0, i32 1 ; <%struct.SnProcess**> [#uses=1]
  store %struct.SnProcess* %tmp8, %struct.SnProcess** %tmp10
  %tmp11 = load i8** %mem                         ; <i8*> [#uses=1]
  %add.ptr = getelementptr inbounds i8* %tmp11, i64 24 ; <i8*> [#uses=1]
  %tmp12 = load %struct.SnObject** %obj           ; <%struct.SnObject*> [#uses=1]
  %tmp13 = getelementptr inbounds %struct.SnObject* %tmp12, i32 0, i32 2 ; <i8**> [#uses=1]
  store i8* %add.ptr, i8** %tmp13
  %tmp14 = load %struct.SnType** %type.addr       ; <%struct.SnType*> [#uses=1]
  %tmp15 = getelementptr inbounds %struct.SnType* %tmp14, i32 0, i32 1 ; <void (%struct.SnProcess*, %struct.SnObject*)**> [#uses=1]
  %tmp16 = load void (%struct.SnProcess*, %struct.SnObject*)** %tmp15 ; <void (%struct.SnProcess*, %struct.SnObject*)*> [#uses=1]
  %tmp17 = load %struct.SnProcess** %p.addr       ; <%struct.SnProcess*> [#uses=1]
  %tmp18 = load %struct.SnObject** %obj           ; <%struct.SnObject*> [#uses=1]
  call void %tmp16(%struct.SnProcess* %tmp17, %struct.SnObject* %tmp18)
  %tmp19 = load %struct.SnObject** %obj           ; <%struct.SnObject*> [#uses=1]
  store %struct.SnObject* %tmp19, %struct.SnObject** %retval
  %1 = load %struct.SnObject** %retval            ; <%struct.SnObject*> [#uses=1]
  ret %struct.SnObject* %1
}

declare i8* @malloc(i64)
