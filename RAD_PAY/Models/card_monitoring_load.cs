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
    
    public partial class card_monitoring_load
    {
        public long id { get; set; }
        public Nullable<long> card_id { get; set; }
        public Nullable<System.DateTime> from_date { get; set; }
        public Nullable<System.DateTime> to_date { get; set; }
        public Nullable<System.DateTime> ts { get; set; }
        public Nullable<int> status { get; set; }
    
        public virtual card card { get; set; }
    }
}
