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
    
    public partial class merchant_group
    {
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2214:DoNotCallOverridableMethodsInConstructors")]
        public merchant_group()
        {
            this.merchant_bonus = new HashSet<merchant_bonus>();
        }
    
        public int id { get; set; }
        public string name { get; set; }
        public Nullable<int> position { get; set; }
        public string name_uzb { get; set; }
        public string icon_path { get; set; }
        public Nullable<long> icon_id { get; set; }
    
        public virtual icon icon { get; set; }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2227:CollectionPropertiesShouldBeReadOnly")]
        public virtual ICollection<merchant_bonus> merchant_bonus { get; set; }
    }
}
