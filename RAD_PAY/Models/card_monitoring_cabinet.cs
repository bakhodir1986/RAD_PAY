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
    
    public partial class card_monitoring_cabinet
    {
        public long id { get; set; }
        public long card_id { get; set; }
        public Nullable<int> monitoring_flag { get; set; }
        public Nullable<System.DateTime> add_ts { get; set; }
        public Nullable<System.DateTime> start_date { get; set; }
        public Nullable<System.DateTime> end_date { get; set; }
        public Nullable<long> purchase_id { get; set; }
        public Nullable<int> status { get; set; }
        public Nullable<System.DateTime> off_ts { get; set; }
        public Nullable<long> periodic_id { get; set; }
        public Nullable<long> uid { get; set; }
    
        public virtual card card { get; set; }
        public virtual user user { get; set; }
    }
}
