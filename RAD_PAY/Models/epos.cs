//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated from a template.
//
//     Manual changes to this file may cause unexpected behavior in your application.
//     Manual changes to this file will be overwritten if the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

namespace RAD_PAY.Models
{
    using System;
    using System.Collections.Generic;
    
    public partial class epos
    {
        public long id { get; set; }
        public string name { get; set; }
        public string merchant_id { get; set; }
        public string terminal_id { get; set; }
        public Nullable<int> port { get; set; }
        public Nullable<int> bank_id { get; set; }
        public Nullable<int> a { get; set; }
        public Nullable<int> b { get; set; }
        public Nullable<int> c { get; set; }
        public Nullable<int> d { get; set; }
    }
}