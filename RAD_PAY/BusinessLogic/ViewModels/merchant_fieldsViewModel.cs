using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_fieldsViewModel
    {
        public int fid { get; set; }
        public int? mid { get; set; }
        public string label { get; set; }
        public int? type { get; set; }
        public int? input_digit { get; set; }
        public int? position { get; set; }
        public int? input_letter { get; set; }
        public string prefix_label { get; set; }
        public string label_uz1 { get; set; }
        public string label_uz2 { get; set; }
        public int? min_length { get; set; }
        public int? max_length { get; set; }
        public int? parent_fid { get; set; }
        public string param_name { get; set; }
        public int? usage { get; set; }
    }
}