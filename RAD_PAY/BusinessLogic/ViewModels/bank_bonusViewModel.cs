using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class bank_bonusViewModel
    {
        public long id { get; set; }
        public long bank_id { get; set; }
        public long? min_amount { get; set; }
        public int? percent { get; set; }
        public DateTime? start_date { get; set; }
        public DateTime? end_date { get; set; }
        public int? status { get; set; }
        public double? longitude { get; set; }
        public double? latitude { get; set; }
        public string description { get; set; }
        public long? bonus_amount { get; set; }
    }
}