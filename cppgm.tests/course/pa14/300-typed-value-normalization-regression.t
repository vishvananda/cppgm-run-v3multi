case cv-unsigned
let-arg A value const uint -1
type template Holder A

case alias-unsigned
let-type U uint
let-arg A value U -1
type template Holder A

case dependent-unsigned
let-type P template-param 0
let-arg A dependent-value P uint -1
type template Holder A

case nested-entity-parameter-isolation
let-entity E function p::C::f int
let-arg A entity-address E
let-type H template p::Holder A
function p::use
param H
